/* log.c --- retrieving log messages
 *
 * ====================================================================
 *    Licensed to the Apache Software Foundation (ASF) under one
 *    or more contributor license agreements.  See the NOTICE file
 *    distributed with this work for additional information
 *    regarding copyright ownership.  The ASF licenses this file
 *    to you under the Apache License, Version 2.0 (the
 *    "License"); you may not use this file except in compliance
 *    with the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing,
 *    software distributed under the License is distributed on an
 *    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *    KIND, either express or implied.  See the License for the
 *    specific language governing permissions and limitations
 *    under the License.
 * ====================================================================
 */


#include <stdlib.h>
#define APR_WANT_STRFUNC
#include <apr_want.h>

#include "svn_compat.h"
#include "svn_private_config.h"
#include "svn_hash.h"
#include "svn_pools.h"
#include "svn_error.h"
#include "svn_path.h"
#include "svn_fs.h"
#include "svn_repos.h"
#include "svn_string.h"
#include "svn_sorts.h"
#include "svn_props.h"
#include "svn_mergeinfo.h"
#include "repos.h"
#include "private/svn_fspath.h"
#include "private/svn_fs_private.h"
#include "private/svn_mergeinfo_private.h"
#include "private/svn_subr_private.h"
#include "private/svn_sorts_private.h"



svn_error_t *
svn_repos_check_revision_access(svn_repos_revision_access_level_t *access_level,
                                svn_repos_t *repos,
                                svn_revnum_t revision,
                                svn_repos_authz_func_t authz_read_func,
                                void *authz_read_baton,
                                apr_pool_t *pool)
{
  svn_fs_t *fs = svn_repos_fs(repos);
  svn_fs_root_t *rev_root;
  apr_hash_t *changes;
  apr_hash_index_t *hi;
  svn_boolean_t found_readable = FALSE;
  svn_boolean_t found_unreadable = FALSE;
  apr_pool_t *subpool;

  /* By default, we'll grant full read access to REVISION. */
  *access_level = svn_repos_revision_access_full;

  /* No auth-checking function?  We're done. */
  if (! authz_read_func)
    return SVN_NO_ERROR;

  /* Fetch the changes associated with REVISION. */
  SVN_ERR(svn_fs_revision_root(&rev_root, fs, revision, pool));
  SVN_ERR(svn_fs_paths_changed2(&changes, rev_root, pool));

  /* No changed paths?  We're done. */
  if (apr_hash_count(changes) == 0)
    return SVN_NO_ERROR;

  /* Otherwise, we have to check the readability of each changed
     path, or at least enough to answer the question asked. */
  subpool = svn_pool_create(pool);
  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    {
      const char *key = svn__apr_hash_index_key(hi);
      svn_fs_path_change2_t *change = svn__apr_hash_index_val(hi);
      svn_boolean_t readable;

      svn_pool_clear(subpool);

      SVN_ERR(authz_read_func(&readable, rev_root, key,
                              authz_read_baton, subpool));
      if (! readable)
        found_unreadable = TRUE;
      else
        found_readable = TRUE;

      /* If we have at least one of each (readable/unreadable), we
         have our answer. */
      if (found_readable && found_unreadable)
        goto decision;

      switch (change->change_kind)
        {
        case svn_fs_path_change_add:
        case svn_fs_path_change_replace:
        case svn_fs_path_change_move:
        case svn_fs_path_change_movereplace:
          {
            const char *copyfrom_path;
            svn_revnum_t copyfrom_rev;

            SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
                                       rev_root, key, subpool));
            if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev))
              {
                svn_fs_root_t *copyfrom_root;
                SVN_ERR(svn_fs_revision_root(&copyfrom_root, fs,
                                             copyfrom_rev, subpool));
                SVN_ERR(authz_read_func(&readable,
                                        copyfrom_root, copyfrom_path,
                                        authz_read_baton, subpool));
                if (! readable)
                  found_unreadable = TRUE;

                /* If we have at least one of each (readable/unreadable), we
                   have our answer. */
                if (found_readable && found_unreadable)
                  goto decision;
              }
          }
          break;

        case svn_fs_path_change_delete:
        case svn_fs_path_change_modify:
        default:
          break;
        }
    }

 decision:
  svn_pool_destroy(subpool);

  /* Either every changed path was unreadable... */
  if (! found_readable)
    *access_level = svn_repos_revision_access_none;

  /* ... or some changed path was unreadable... */
  else if (found_unreadable)
    *access_level = svn_repos_revision_access_partial;

  /* ... or every changed path was readable (the default). */
  return SVN_NO_ERROR;
}

/* Return TRUE, if CHANGE deleted the node previously found at its target
   path. */
static svn_boolean_t
is_deletion(svn_log_changed_path2_t *change)
{
  /* We need a 'N' action here ... */
  return change->action == 'E'
      || change->action == 'R'
      || change->action == 'D';
}

/* Change all moves in CHANGES to ADD.  Use POOL for temporary allocations.
 */
static void
turn_moves_into_copies(apr_hash_t *changes,
                       apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    {
      svn_log_changed_path2_t *change = svn__apr_hash_index_val(hi);

      switch (change->action)
        {
          case 'V':
            change->action = 'A';
            break;

          case 'E':
            change->action = 'R';
            break;

          default:
            break;
        }
    }
}

/* Replace ADDs with MOVes, if they are unique, have a matching deletion
 * and if the copy-from revision is REVISION-1.  Use POOL for temporary
 * allocations.
 */
static void
turn_unique_copies_into_moves(apr_hash_t *changes,
                              svn_revnum_t revision,
                              apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  apr_hash_t *unique_copy_sources;
  const char **sources;
  int i;

  /* find all copy-from paths (ADD and MOV alike) */

  svn_boolean_t any_deletion = FALSE;
  apr_array_header_t *copy_sources
    = apr_array_make(pool, apr_hash_count(changes), sizeof(const char*));

  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    {
      svn_log_changed_path2_t *change = svn__apr_hash_index_val(hi);

      if (change->copyfrom_path && change->copyfrom_rev == revision-1)
        APR_ARRAY_PUSH(copy_sources, const char *)
          = change->copyfrom_path;

      any_deletion |= is_deletion(change);
    }

  /* no suitable copy-from or no deletion -> no moves */

  if (!copy_sources->nelts || !any_deletion)
    return;

  /* identify copy-from paths that have been mentioned exactly once */
  svn_sort__array(copy_sources, svn_sort_compare_paths);

  unique_copy_sources = apr_hash_make(pool);
  sources = (const char **)copy_sources->elts;
  for (i = 0; i < copy_sources->nelts; ++i)
    if (   (i == 0 || strcmp(sources[i-1], sources[i]))
        && (i == copy_sources->nelts-1 || strcmp(sources[i+1], sources[i])))
      {
        svn_hash_sets(unique_copy_sources, sources[i], sources[i]);
      }

  /* no unique copy-from paths -> no moves */

  if (!apr_hash_count(unique_copy_sources))
    return;

  /* Replace all additions, replacements with a unique copy-from path,
     the correct copy-from rev and a matching deletion in this revision,
     with moves and move-replacements, respectively. */

  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    {
      svn_log_changed_path2_t *change = svn__apr_hash_index_val(hi);
      svn_log_changed_path2_t *copy_from_change;

      if (   change->copyfrom_rev != revision-1
          || !change->copyfrom_path
          || !svn_hash_gets(unique_copy_sources, change->copyfrom_path))
        continue;

      copy_from_change = svn_hash_gets(changes, change->copyfrom_path);
      if (!copy_from_change || !is_deletion(copy_from_change))
        continue;

      /* There is a deletion of the ADD's copy-from path in *REVISION*.
         This can either be the same as in REVISION-1 (o.k.) or must have
         been replaced by some other node.  However, that would imply that
         it still got deleted as part of the replacement, i.e. both cases
         are o.k. */

      switch (change->action)
        {
          case 'A':
            change->action = 'V';
            break;

          case 'R':
            change->action = 'E';
            break;

          default:
            break;
        }
    }
}

/* Store as keys in CHANGED the paths of all node in ROOT that show a
 * significant change.  "Significant" means that the text or
 * properties of the node were changed, or that the node was added or
 * deleted.
 *
 * The CHANGED hash set and its keys and values are allocated in POOL;
 * keys are const char * paths and values are svn_log_changed_path_t.
 *
 * To prevent changes from being processed over and over again, the
 * changed paths for ROOT may be passed in PREFETCHED_CHANGES.  If the
 * latter is NULL, we will request the list inside this function using
 * the specified MOVE_BEHAVIOR.
 *
 * If optional AUTHZ_READ_FUNC is non-NULL, then use it (with
 * AUTHZ_READ_BATON and FS) to check whether each changed-path (and
 * copyfrom_path) is readable:
 *
 *     - If some paths are readable and some are not, then silently
 *     omit the unreadable paths from the CHANGED hash, and return
 *     SVN_ERR_AUTHZ_PARTIALLY_READABLE.
 *
 *     - If absolutely every changed-path (and copyfrom_path) is
 *     unreadable, then return an empty CHANGED hash and
 *     SVN_ERR_AUTHZ_UNREADABLE.  (This is to distinguish a revision
 *     which truly has no changed paths from a revision in which all
 *     paths are unreadable.)
 */
static svn_error_t *
detect_changed(apr_hash_t **changed,
               svn_fs_root_t *root,
               svn_fs_t *fs,
               apr_hash_t *prefetched_changes,
               svn_move_behavior_t move_behavior,
               svn_repos_authz_func_t authz_read_func,
               void *authz_read_baton,
               apr_pool_t *pool)
{
  apr_hash_t *changes = prefetched_changes;
  apr_hash_index_t *hi;
  apr_pool_t *subpool;
  svn_boolean_t found_readable = FALSE;
  svn_boolean_t found_unreadable = FALSE;

  *changed = svn_hash__make(pool);
  if (changes == NULL)
    SVN_ERR(svn_fs_paths_changed2(&changes, root, pool));

  if (apr_hash_count(changes) == 0)
    /* No paths changed in this revision?  Uh, sure, I guess the
       revision is readable, then.  */
    return SVN_NO_ERROR;

  subpool = svn_pool_create(pool);

  for (hi = apr_hash_first(pool, changes); hi; hi = apr_hash_next(hi))
    {
      /* NOTE:  Much of this loop is going to look quite similar to
         svn_repos_check_revision_access(), but we have to do more things
         here, so we'll live with the duplication. */
      const char *path = svn__apr_hash_index_key(hi);
      apr_ssize_t path_len = svn__apr_hash_index_klen(hi);
      svn_fs_path_change2_t *change = svn__apr_hash_index_val(hi);
      char action;
      svn_log_changed_path2_t *item;

      svn_pool_clear(subpool);

      /* Skip path if unreadable. */
      if (authz_read_func)
        {
          svn_boolean_t readable;
          SVN_ERR(authz_read_func(&readable,
                                  root, path,
                                  authz_read_baton, subpool));
          if (! readable)
            {
              found_unreadable = TRUE;
              continue;
            }
        }

      /* At least one changed-path was readable. */
      found_readable = TRUE;

      switch (change->change_kind)
        {
        case svn_fs_path_change_reset:
          continue;

        case svn_fs_path_change_add:
          action = 'A';
          break;

        case svn_fs_path_change_replace:
          action = 'R';
          break;

        case svn_fs_path_change_delete:
          action = 'D';
          break;

        case svn_fs_path_change_move:
          action = 'V';
          break;

        case svn_fs_path_change_movereplace:
          action = 'E';
          break;

        case svn_fs_path_change_modify:
        default:
          action = 'M';
          break;
        }

      item = svn_log_changed_path2_create(pool);
      item->action = action;
      item->node_kind = change->node_kind;
      item->copyfrom_rev = SVN_INVALID_REVNUM;
      item->text_modified = change->text_mod ? svn_tristate_true
                                             : svn_tristate_false;
      item->props_modified = change->prop_mod ? svn_tristate_true
                                              : svn_tristate_false;

      /* Pre-1.6 revision files don't store the change path kind, so fetch
         it manually. */
      if (item->node_kind == svn_node_unknown)
        {
          svn_fs_root_t *check_root = root;
          const char *check_path = path;

          /* Deleted items don't exist so check earlier revision.  We
             know the parent must exist and could be a copy */
          if (change->change_kind == svn_fs_path_change_delete)
            {
              svn_fs_history_t *history;
              svn_revnum_t prev_rev;
              const char *parent_path, *name;

              svn_fspath__split(&parent_path, &name, path, subpool);

              SVN_ERR(svn_fs_node_history(&history, root, parent_path,
                                          subpool));

              /* Two calls because the first call returns the original
                 revision as the deleted child means it is 'interesting' */
              SVN_ERR(svn_fs_history_prev(&history, history, TRUE, subpool));
              SVN_ERR(svn_fs_history_prev(&history, history, TRUE, subpool));

              SVN_ERR(svn_fs_history_location(&parent_path, &prev_rev, history,
                                              subpool));
              SVN_ERR(svn_fs_revision_root(&check_root, fs, prev_rev, subpool));
              check_path = svn_fspath__join(parent_path, name, subpool);
            }

          SVN_ERR(svn_fs_check_path(&item->node_kind, check_root, check_path,
                                    subpool));
        }


      if (   (action == 'A') || (action == 'R')
          || (action == 'V') || (action == 'E'))
        {
          const char *copyfrom_path = change->copyfrom_path;
          svn_revnum_t copyfrom_rev = change->copyfrom_rev;

          /* the following is a potentially expensive operation since on FSFS
             we will follow the DAG from ROOT to PATH and that requires
             actually reading the directories along the way. */
          if (!change->copyfrom_known)
            SVN_ERR(svn_fs_copied_from(&copyfrom_rev, &copyfrom_path,
                                      root, path, subpool));

          if (copyfrom_path && SVN_IS_VALID_REVNUM(copyfrom_rev))
            {
              svn_boolean_t readable = TRUE;

              if (authz_read_func)
                {
                  svn_fs_root_t *copyfrom_root;

                  SVN_ERR(svn_fs_revision_root(&copyfrom_root, fs,
                                               copyfrom_rev, subpool));
                  SVN_ERR(authz_read_func(&readable,
                                          copyfrom_root, copyfrom_path,
                                          authz_read_baton, subpool));
                  if (! readable)
                    found_unreadable = TRUE;
                }

              if (readable)
                {
                  item->copyfrom_path = apr_pstrdup(pool, copyfrom_path);
                  item->copyfrom_rev = copyfrom_rev;
                }
            }
        }

      apr_hash_set(*changed, apr_pstrmemdup(pool, path, path_len), path_len,
                   item);
    }

  svn_pool_destroy(subpool);

  if (! found_readable)
    /* Every changed-path was unreadable. */
    return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE,
                            NULL, NULL);

  /* at least some paths are readable.  Post-process them. */
  switch(move_behavior)
    {
      case svn_move_behavior_no_moves:
        turn_moves_into_copies(*changed, pool);
        break;

      case svn_move_behavior_auto_moves:
        turn_unique_copies_into_moves(*changed,
                                      svn_fs_revision_root_revision(root),
                                      pool);
        break;

      default:
        break;
    }

  if (found_unreadable)
    /* At least one changed-path was unreadable. */
    return svn_error_create(SVN_ERR_AUTHZ_PARTIALLY_READABLE,
                            NULL, NULL);

  /* Every changed-path was readable. */
  return SVN_NO_ERROR;
}

/* This is used by svn_repos_get_logs to keep track of multiple
 * path history information while working through history.
 *
 * The two pools are swapped after each iteration through history because
 * to get the next history requires the previous one.
 */
struct path_info
{
  svn_stringbuf_t *path;
  svn_revnum_t history_rev;
  svn_boolean_t done;
  svn_boolean_t first_time;

  /* If possible, we like to keep open the history object for each path,
     since it avoids needed to open and close it many times as we walk
     backwards in time.  To do so we need two pools, so that we can clear
     one each time through.  If we're not holding the history open for
     this path then these three pointers will be NULL. */
  svn_fs_history_t *hist;
  apr_pool_t *newpool;
  apr_pool_t *oldpool;
};

/* Advance to the next history for the path.
 *
 * If INFO->HIST is not NULL we do this using that existing history object,
 * otherwise we open a new one.
 *
 * If no more history is available or the history revision is less
 * (earlier) than START, or the history is not available due
 * to authorization, then INFO->DONE is set to TRUE.
 *
 * A STRICT value of FALSE will indicate to follow history across copied
 * paths.
 *
 * If optional AUTHZ_READ_FUNC is non-NULL, then use it (with
 * AUTHZ_READ_BATON and FS) to check whether INFO->PATH is still readable if
 * we do indeed find more history for the path.
 */
static svn_error_t *
get_history(struct path_info *info,
            svn_fs_t *fs,
            svn_boolean_t strict,
            svn_repos_authz_func_t authz_read_func,
            void *authz_read_baton,
            svn_revnum_t start,
            apr_pool_t *pool)
{
  svn_fs_root_t *history_root = NULL;
  svn_fs_history_t *hist;
  apr_pool_t *subpool;
  const char *path;

  if (info->hist)
    {
      subpool = info->newpool;

      SVN_ERR(svn_fs_history_prev(&info->hist, info->hist, ! strict, subpool));

      hist = info->hist;
    }
  else
    {
      subpool = svn_pool_create(pool);

      /* Open the history located at the last rev we were at. */
      SVN_ERR(svn_fs_revision_root(&history_root, fs, info->history_rev,
                                   subpool));

      SVN_ERR(svn_fs_node_history(&hist, history_root, info->path->data,
                                  subpool));

      SVN_ERR(svn_fs_history_prev(&hist, hist, ! strict, subpool));

      if (info->first_time)
        info->first_time = FALSE;
      else
        SVN_ERR(svn_fs_history_prev(&hist, hist, ! strict, subpool));
    }

  if (! hist)
    {
      svn_pool_destroy(subpool);
      if (info->oldpool)
        svn_pool_destroy(info->oldpool);
      info->done = TRUE;
      return SVN_NO_ERROR;
    }

  /* Fetch the location information for this history step. */
  SVN_ERR(svn_fs_history_location(&path, &info->history_rev,
                                  hist, subpool));

  svn_stringbuf_set(info->path, path);

  /* If this history item predates our START revision then
     don't fetch any more for this path. */
  if (info->history_rev < start)
    {
      svn_pool_destroy(subpool);
      if (info->oldpool)
        svn_pool_destroy(info->oldpool);
      info->done = TRUE;
      return SVN_NO_ERROR;
    }

  /* Is the history item readable?  If not, done with path. */
  if (authz_read_func)
    {
      svn_boolean_t readable;
      SVN_ERR(svn_fs_revision_root(&history_root, fs,
                                   info->history_rev,
                                   subpool));
      SVN_ERR(authz_read_func(&readable, history_root,
                              info->path->data,
                              authz_read_baton,
                              subpool));
      if (! readable)
        info->done = TRUE;
    }

  if (! info->hist)
    {
      svn_pool_destroy(subpool);
    }
  else
    {
      apr_pool_t *temppool = info->oldpool;
      info->oldpool = info->newpool;
      svn_pool_clear(temppool);
      info->newpool = temppool;
    }

  return SVN_NO_ERROR;
}

/* Set INFO->HIST to the next history for the path *if* there is history
 * available and INFO->HISTORY_REV is equal to or greater than CURRENT.
 *
 * *CHANGED is set to TRUE if the path has history in the CURRENT revision,
 * otherwise it is not touched.
 *
 * If we do need to get the next history revision for the path, call
 * get_history to do it -- see it for details.
 */
static svn_error_t *
check_history(svn_boolean_t *changed,
              struct path_info *info,
              svn_fs_t *fs,
              svn_revnum_t current,
              svn_boolean_t strict,
              svn_repos_authz_func_t authz_read_func,
              void *authz_read_baton,
              svn_revnum_t start,
              apr_pool_t *pool)
{
  /* If we're already done with histories for this path,
     don't try to fetch any more. */
  if (info->done)
    return SVN_NO_ERROR;

  /* If the last rev we got for this path is less than CURRENT,
     then just return and don't fetch history for this path.
     The caller will get to this rev eventually or else reach
     the limit. */
  if (info->history_rev < current)
    return SVN_NO_ERROR;

  /* If the last rev we got for this path is equal to CURRENT
     then set *CHANGED to true and get the next history
     rev where this path was changed. */
  *changed = TRUE;
  return get_history(info, fs, strict, authz_read_func,
                     authz_read_baton, start, pool);
}

/* Return the next interesting revision in our list of HISTORIES. */
static svn_revnum_t
next_history_rev(const apr_array_header_t *histories)
{
  svn_revnum_t next_rev = SVN_INVALID_REVNUM;
  int i;

  for (i = 0; i < histories->nelts; ++i)
    {
      struct path_info *info = APR_ARRAY_IDX(histories, i,
                                             struct path_info *);
      if (info->done)
        continue;
      if (info->history_rev > next_rev)
        next_rev = info->history_rev;
    }

  return next_rev;
}

/* Set *DELETED_MERGEINFO_CATALOG and *ADDED_MERGEINFO_CATALOG to
   catalogs describing how mergeinfo values on paths (which are the
   keys of those catalogs) were changed in REV.  If *PREFETCHED_CHANGES
   already contains the changed paths for REV, use that.  Otherwise,
   request that data and return it in *PREFETCHED_CHANGES. */
/* ### TODO: This would make a *great*, useful public function,
   ### svn_repos_fs_mergeinfo_changed()!  -- cmpilato  */
static svn_error_t *
fs_mergeinfo_changed(svn_mergeinfo_catalog_t *deleted_mergeinfo_catalog,
                     svn_mergeinfo_catalog_t *added_mergeinfo_catalog,
                     apr_hash_t **prefetched_changes,
                     svn_fs_t *fs,
                     svn_revnum_t rev,
                     apr_pool_t *result_pool,
                     apr_pool_t *scratch_pool)
{
  svn_fs_root_t *root;
  apr_pool_t *iterpool;
  apr_hash_index_t *hi;

  /* Initialize return variables. */
  *deleted_mergeinfo_catalog = svn_hash__make(result_pool);
  *added_mergeinfo_catalog = svn_hash__make(result_pool);

  /* Revision 0 has no mergeinfo and no mergeinfo changes. */
  if (rev == 0)
    return SVN_NO_ERROR;

  /* We're going to use the changed-paths information for REV to
     narrow down our search. */
  SVN_ERR(svn_fs_revision_root(&root, fs, rev, scratch_pool));
  if (*prefetched_changes == NULL)
    SVN_ERR(svn_fs_paths_changed2(prefetched_changes, root, scratch_pool));

  /* No changed paths?  We're done. */
  if (apr_hash_count(*prefetched_changes) == 0)
    return SVN_NO_ERROR;

  /* Loop over changes, looking for anything that might carry an
     svn:mergeinfo change and is one of our paths of interest, or a
     child or [grand]parent directory thereof. */
  iterpool = svn_pool_create(scratch_pool);
  for (hi = apr_hash_first(scratch_pool, *prefetched_changes);
       hi;
       hi = apr_hash_next(hi))
    {
      const char *changed_path = svn__apr_hash_index_key(hi);
      svn_fs_path_change2_t *change = svn__apr_hash_index_val(hi);
      const char *base_path = NULL;
      svn_revnum_t base_rev = SVN_INVALID_REVNUM;
      svn_fs_root_t *base_root = NULL;
      svn_string_t *prev_mergeinfo_value = NULL, *mergeinfo_value;

      svn_pool_clear(iterpool);

      /* If there was no mergeinfo change on this item, ignore it. */
      if (change->mergeinfo_mod == svn_tristate_false)
        continue;

      /* If there was no property change on this item, ignore it. */
      if (! change->prop_mod)
        continue;

      switch (change->change_kind)
        {

        /* ### TODO: Can the add, replace, and modify cases be joined
           ### together to all use svn_repos__prev_location()?  The
           ### difference would be the fallback case (path/rev-1 for
           ### modifies, NULL otherwise).  -- cmpilato  */

        /* If the path was added or replaced, see if it was created via
           copy.  If so, set BASE_REV/BASE_PATH to its previous location.
           If not, there's no previous location to examine -- leave
           BASE_REV/BASE_PATH = -1/NULL.  */
        case svn_fs_path_change_add:
        case svn_fs_path_change_replace:
        case svn_fs_path_change_move:
        case svn_fs_path_change_movereplace:
          {
            SVN_ERR(svn_fs_copied_from(&base_rev, &base_path,
                                       root, changed_path, iterpool));
            break;
          }

        /* If the path was merely modified, see if its previous
           location was affected by a copy which happened in this
           revision before assuming it holds the same path it did the
           previous revision. */
        case svn_fs_path_change_modify:
          {
            svn_revnum_t appeared_rev;

            SVN_ERR(svn_repos__prev_location(&appeared_rev, &base_path,
                                             &base_rev, fs, rev,
                                             changed_path, iterpool));

            /* If this path isn't the result of a copy that occurred
               in this revision, we can find the previous version of
               it in REV - 1 at the same path. */
            if (! (base_path && SVN_IS_VALID_REVNUM(base_rev)
                   && (appeared_rev == rev)))
              {
                base_path = changed_path;
                base_rev = rev - 1;
              }
            break;
          }

        /* We don't care about any of the other cases. */
        case svn_fs_path_change_delete:
        case svn_fs_path_change_reset:
        default:
          continue;
        }

      /* If there was a base location, fetch its mergeinfo property value. */
      if (base_path && SVN_IS_VALID_REVNUM(base_rev))
        {
          SVN_ERR(svn_fs_revision_root(&base_root, fs, base_rev, iterpool));
          SVN_ERR(svn_fs_node_prop(&prev_mergeinfo_value, base_root, base_path,
                                   SVN_PROP_MERGEINFO, iterpool));
        }

      /* Now fetch the current (as of REV) mergeinfo property value. */
      SVN_ERR(svn_fs_node_prop(&mergeinfo_value, root, changed_path,
                               SVN_PROP_MERGEINFO, iterpool));

      /* No mergeinfo on either the new or previous location?  Just
         skip it.  (If there *was* a change, it would have been in
         inherited mergeinfo only, which should be picked up by the
         iteration of this loop that finds the parent paths that
         really got changed.)  */
      if (! (mergeinfo_value || prev_mergeinfo_value))
        continue;

      /* If mergeinfo was explicitly added or removed on this path, we
         need to check to see if that was a real semantic change of
         meaning.  So, fill in the "missing" mergeinfo value with the
         inherited mergeinfo for that path/revision.  */
      if (prev_mergeinfo_value && (! mergeinfo_value))
        {
          svn_mergeinfo_t tmp_mergeinfo;

          SVN_ERR(svn_fs__get_mergeinfo_for_path(&tmp_mergeinfo,
                                                 root, changed_path,
                                                 svn_mergeinfo_inherited, TRUE,
                                                 iterpool, iterpool));
          if (tmp_mergeinfo)
            SVN_ERR(svn_mergeinfo_to_string(&mergeinfo_value,
                                            tmp_mergeinfo,
                                            iterpool));
        }
      else if (mergeinfo_value && (! prev_mergeinfo_value)
               && base_path && SVN_IS_VALID_REVNUM(base_rev))
        {
          svn_mergeinfo_t tmp_mergeinfo;

          SVN_ERR(svn_fs__get_mergeinfo_for_path(&tmp_mergeinfo,
                                                 base_root, base_path,
                                                 svn_mergeinfo_inherited, TRUE,
                                                 iterpool, iterpool));
          if (tmp_mergeinfo)
            SVN_ERR(svn_mergeinfo_to_string(&prev_mergeinfo_value,
                                            tmp_mergeinfo,
                                            iterpool));
        }

      /* If the old and new mergeinfo differ in any way, store the
         before and after mergeinfo values in our return hashes. */
      if ((prev_mergeinfo_value && (! mergeinfo_value))
          || ((! prev_mergeinfo_value) && mergeinfo_value)
          || (prev_mergeinfo_value && mergeinfo_value
              && (! svn_string_compare(mergeinfo_value,
                                       prev_mergeinfo_value))))
        {
          svn_mergeinfo_t prev_mergeinfo = NULL, mergeinfo = NULL;
          svn_mergeinfo_t deleted, added;
          const char *hash_path;

          if (mergeinfo_value)
            SVN_ERR(svn_mergeinfo_parse(&mergeinfo,
                                        mergeinfo_value->data, iterpool));
          if (prev_mergeinfo_value)
            SVN_ERR(svn_mergeinfo_parse(&prev_mergeinfo,
                                        prev_mergeinfo_value->data, iterpool));
          SVN_ERR(svn_mergeinfo_diff2(&deleted, &added, prev_mergeinfo,
                                      mergeinfo, FALSE, result_pool,
                                      iterpool));

          /* Toss interesting stuff into our return catalogs. */
          hash_path = apr_pstrdup(result_pool, changed_path);
          svn_hash_sets(*deleted_mergeinfo_catalog, hash_path, deleted);
          svn_hash_sets(*added_mergeinfo_catalog, hash_path, added);
        }
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}


/* Determine what (if any) mergeinfo for PATHS was modified in
   revision REV, returning the differences for added mergeinfo in
   *ADDED_MERGEINFO and deleted mergeinfo in *DELETED_MERGEINFO.
   If *PREFETCHED_CHANGES already contains the changed paths for
   REV, use that.  Otherwise, request that data and return it in
   *PREFETCHED_CHANGES. */
static svn_error_t *
get_combined_mergeinfo_changes(svn_mergeinfo_t *added_mergeinfo,
                               svn_mergeinfo_t *deleted_mergeinfo,
                               apr_hash_t **prefetched_changes,
                               svn_fs_t *fs,
                               const apr_array_header_t *paths,
                               svn_revnum_t rev,
                               apr_pool_t *result_pool,
                               apr_pool_t *scratch_pool)
{
  svn_mergeinfo_catalog_t added_mergeinfo_catalog, deleted_mergeinfo_catalog;
  apr_hash_index_t *hi;
  svn_fs_root_t *root;
  apr_pool_t *iterpool;
  int i;
  svn_error_t *err;

  /* Initialize return value. */
  *added_mergeinfo = svn_hash__make(result_pool);
  *deleted_mergeinfo = svn_hash__make(result_pool);

  /* If we're asking about revision 0, there's no mergeinfo to be found. */
  if (rev == 0)
    return SVN_NO_ERROR;

  /* No paths?  No mergeinfo. */
  if (! paths->nelts)
    return SVN_NO_ERROR;

  /* Create a work subpool and get a root for REV. */
  SVN_ERR(svn_fs_revision_root(&root, fs, rev, scratch_pool));

  /* Fetch the mergeinfo changes for REV. */
  err = fs_mergeinfo_changed(&deleted_mergeinfo_catalog,
                             &added_mergeinfo_catalog,
                             prefetched_changes,
                             fs, rev,
                             scratch_pool, scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_MERGEINFO_PARSE_ERROR)
        {
          /* Issue #3896: If invalid mergeinfo is encountered the
             best we can do is ignore it and act as if there were
             no mergeinfo modifications. */
          svn_error_clear(err);
          return SVN_NO_ERROR;
        }
      else
        {
          return svn_error_trace(err);
        }
    }

  /* In most revisions, there will be no mergeinfo change at all. */
  if (   apr_hash_count(deleted_mergeinfo_catalog) == 0
      && apr_hash_count(added_mergeinfo_catalog) == 0)
    return SVN_NO_ERROR;
  
  /* Check our PATHS for any changes to their inherited mergeinfo.
     (We deal with changes to mergeinfo directly *on* the paths in the
     following loop.)  */
  iterpool = svn_pool_create(scratch_pool);
  for (i = 0; i < paths->nelts; i++)
    {
      const char *path = APR_ARRAY_IDX(paths, i, const char *);
      const char *prev_path;
      svn_revnum_t appeared_rev, prev_rev;
      svn_fs_root_t *prev_root;
      svn_mergeinfo_t prev_mergeinfo, mergeinfo, deleted, added,
        prev_inherited_mergeinfo, inherited_mergeinfo;

      svn_pool_clear(iterpool);

      /* If this path is represented in the changed-mergeinfo hashes,
         we'll deal with it in the loop below. */
      if (svn_hash_gets(deleted_mergeinfo_catalog, path))
        continue;

      /* Figure out what path/rev to compare against.  Ignore
         not-found errors returned by the filesystem.  */
      err = svn_repos__prev_location(&appeared_rev, &prev_path, &prev_rev,
                                     fs, rev, path, iterpool);
      if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
                  err->apr_err == SVN_ERR_FS_NOT_DIRECTORY))
        {
          svn_error_clear(err);
          err = SVN_NO_ERROR;
          continue;
        }
      SVN_ERR(err);

      /* If this path isn't the result of a copy that occurred in this
         revision, we can find the previous version of it in REV - 1
         at the same path. */
      if (! (prev_path && SVN_IS_VALID_REVNUM(prev_rev)
             && (appeared_rev == rev)))
        {
          prev_path = path;
          prev_rev = rev - 1;
        }

      /* Fetch the previous mergeinfo (including inherited stuff) for
         this path.  Ignore not-found errors returned by the
         filesystem or invalid mergeinfo (Issue #3896).*/
      SVN_ERR(svn_fs_revision_root(&prev_root, fs, prev_rev, iterpool));
      err = svn_fs__get_mergeinfo_for_path(&prev_mergeinfo,
                                           prev_root, prev_path,
                                           svn_mergeinfo_inherited, TRUE,
                                           iterpool, iterpool);
      if (err && (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
                  err->apr_err == SVN_ERR_FS_NOT_DIRECTORY ||
                  err->apr_err == SVN_ERR_MERGEINFO_PARSE_ERROR))
        {
          svn_error_clear(err);
          err = SVN_NO_ERROR;
          continue;
        }
      SVN_ERR(err);

      /* Issue #4022 'svn log -g interprets change in inherited mergeinfo due
         to move as a merge': A copy where the source and destination inherit
         mergeinfo from the same parent means the inherited mergeinfo of the
         source and destination will differ, but this diffrence is not
         indicative of a merge unless the mergeinfo on the inherited parent
         has actually changed.

         To check for this we must fetch the "raw" previous inherited
         mergeinfo and the "raw" mergeinfo @REV then compare these. */
      SVN_ERR(svn_fs__get_mergeinfo_for_path(&prev_inherited_mergeinfo,
                                             prev_root, prev_path,
                                             svn_mergeinfo_nearest_ancestor,
                                             FALSE, /* adjust_inherited_mergeinfo */
                                             iterpool, iterpool));

      /* Fetch the current mergeinfo (as of REV, and including
         inherited stuff) for this path. */
      SVN_ERR(svn_fs__get_mergeinfo_for_path(&mergeinfo,
                                             root, path,
                                             svn_mergeinfo_inherited, TRUE,
                                             iterpool, iterpool));

      /* Issue #4022 again, fetch the raw inherited mergeinfo. */
      SVN_ERR(svn_fs__get_mergeinfo_for_path(&inherited_mergeinfo,
                                             root, path,
                                             svn_mergeinfo_nearest_ancestor,
                                             FALSE, /* adjust_inherited_mergeinfo */
                                             iterpool, iterpool));

      if (!prev_mergeinfo && !mergeinfo)
        continue;

      /* Last bit of issue #4022 checking. */
      if (prev_inherited_mergeinfo && inherited_mergeinfo)
        {
          svn_boolean_t inherits_same_mergeinfo;

          SVN_ERR(svn_mergeinfo__equals(&inherits_same_mergeinfo,
                                        prev_inherited_mergeinfo,
                                        inherited_mergeinfo,
                                        TRUE, iterpool));
          /* If a copy rather than an actual merge brought about an
             inherited mergeinfo change then we are finished. */
          if (inherits_same_mergeinfo)
            continue;
        }
      else
        {
          svn_boolean_t same_mergeinfo;
          SVN_ERR(svn_mergeinfo__equals(&same_mergeinfo,
                                        prev_inherited_mergeinfo,
                                        NULL,
                                        TRUE, iterpool));
          if (same_mergeinfo)
            continue;
        }

      /* Compare, constrast, and combine the results. */
      SVN_ERR(svn_mergeinfo_diff2(&deleted, &added, prev_mergeinfo,
                                  mergeinfo, FALSE, result_pool, iterpool));
      SVN_ERR(svn_mergeinfo_merge2(*deleted_mergeinfo, deleted,
                                   result_pool, iterpool));
      SVN_ERR(svn_mergeinfo_merge2(*added_mergeinfo, added,
                                   result_pool, iterpool));
     }

  /* Merge all the mergeinfos which are, or are children of, one of
     our paths of interest into one giant delta mergeinfo.  */
  for (hi = apr_hash_first(scratch_pool, added_mergeinfo_catalog);
       hi; hi = apr_hash_next(hi))
    {
      const char *changed_path = svn__apr_hash_index_key(hi);
      apr_ssize_t klen = svn__apr_hash_index_klen(hi);
      svn_mergeinfo_t added = svn__apr_hash_index_val(hi);
      svn_mergeinfo_t deleted;

      for (i = 0; i < paths->nelts; i++)
        {
          const char *path = APR_ARRAY_IDX(paths, i, const char *);
          if (! svn_fspath__skip_ancestor(path, changed_path))
            continue;
          svn_pool_clear(iterpool);
          deleted = apr_hash_get(deleted_mergeinfo_catalog, changed_path, klen);
          SVN_ERR(svn_mergeinfo_merge2(*deleted_mergeinfo,
                                       svn_mergeinfo_dup(deleted, result_pool),
                                       result_pool, iterpool));
          SVN_ERR(svn_mergeinfo_merge2(*added_mergeinfo,
                                       svn_mergeinfo_dup(added, result_pool),
                                       result_pool, iterpool));

          break;
        }
    }

  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}


/* Fill LOG_ENTRY with history information in FS at REV. */
static svn_error_t *
fill_log_entry(svn_log_entry_t *log_entry,
               svn_revnum_t rev,
               svn_fs_t *fs,
               apr_hash_t *prefetched_changes,
               svn_boolean_t discover_changed_paths,
               svn_move_behavior_t move_behavior,
               const apr_array_header_t *revprops,
               svn_repos_authz_func_t authz_read_func,
               void *authz_read_baton,
               apr_pool_t *pool)
{
  apr_hash_t *r_props, *changed_paths = NULL;
  svn_boolean_t get_revprops = TRUE, censor_revprops = FALSE;

  /* Discover changed paths if the user requested them
     or if we need to check that they are readable. */
  if ((rev > 0)
      && (authz_read_func || discover_changed_paths))
    {
      svn_fs_root_t *newroot;
      svn_error_t *patherr;

      SVN_ERR(svn_fs_revision_root(&newroot, fs, rev, pool));
      patherr = detect_changed(&changed_paths,
                               newroot, fs, prefetched_changes, move_behavior,
                               authz_read_func, authz_read_baton,
                               pool);

      if (patherr
          && patherr->apr_err == SVN_ERR_AUTHZ_UNREADABLE)
        {
          /* All changed-paths are unreadable, so clear all fields. */
          svn_error_clear(patherr);
          changed_paths = NULL;
          get_revprops = FALSE;
        }
      else if (patherr
               && patherr->apr_err == SVN_ERR_AUTHZ_PARTIALLY_READABLE)
        {
          /* At least one changed-path was unreadable, so censor all
             but author and date.  (The unreadable paths are already
             missing from the hash.) */
          svn_error_clear(patherr);
          censor_revprops = TRUE;
        }
      else if (patherr)
        return patherr;

      /* It may be the case that an authz func was passed in, but
         the user still doesn't want to see any changed-paths. */
      if (! discover_changed_paths)
        changed_paths = NULL;
    }

  if (get_revprops)
    {
      /* User is allowed to see at least some revprops. */
      SVN_ERR(svn_fs_revision_proplist(&r_props, fs, rev, pool));
      if (revprops == NULL)
        {
          /* Requested all revprops... */
          if (censor_revprops)
            {
              /* ... but we can only return author/date. */
              log_entry->revprops = svn_hash__make(pool);
              svn_hash_sets(log_entry->revprops, SVN_PROP_REVISION_AUTHOR,
                            svn_hash_gets(r_props, SVN_PROP_REVISION_AUTHOR));
              svn_hash_sets(log_entry->revprops, SVN_PROP_REVISION_DATE,
                            svn_hash_gets(r_props, SVN_PROP_REVISION_DATE));
            }
          else
            /* ... so return all we got. */
            log_entry->revprops = r_props;
        }
      else
        {
          int i;

          /* Requested only some revprops... */
          
          /* often only the standard revprops got requested and delivered.
             In that case, we can simply pass the hash on. */
          if (revprops->nelts == apr_hash_count(r_props) && !censor_revprops)
            {
              log_entry->revprops = r_props;
              for (i = 0; i < revprops->nelts; i++)
                {
                  const svn_string_t *name
                    = APR_ARRAY_IDX(revprops, i, const svn_string_t *);
                  if (!apr_hash_get(r_props, name->data, name->len))
                    {
                      /* hash does not match list of revprops we want */
                      log_entry->revprops = NULL;
                      break;
                    }
                }
            }

          /* slow, revprop-by-revprop filtering */
          if (log_entry->revprops == NULL)
            for (i = 0; i < revprops->nelts; i++)
              {
                const svn_string_t *name
                  = APR_ARRAY_IDX(revprops, i, const svn_string_t *);
                svn_string_t *value
                  = apr_hash_get(r_props, name->data, name->len);
                if (censor_revprops
                    && !(strncmp(name->data, SVN_PROP_REVISION_AUTHOR,
                                 name->len) == 0
                         || strncmp(name->data, SVN_PROP_REVISION_DATE,
                                    name->len) == 0))
                  /* ... but we can only return author/date. */
                  continue;
                if (log_entry->revprops == NULL)
                  log_entry->revprops = svn_hash__make(pool);
                apr_hash_set(log_entry->revprops, name->data, name->len, value);
              }
        }
    }

  log_entry->changed_paths = changed_paths;
  log_entry->changed_paths2 = changed_paths;
  log_entry->revision = rev;

  return SVN_NO_ERROR;
}

/* Send a log message for REV to RECEIVER with its RECEIVER_BATON.

   FS is used with REV to fetch the interesting history information,
   such as changed paths, revprops, etc.

   The detect_changed function is used if either AUTHZ_READ_FUNC is
   not NULL, or if DISCOVER_CHANGED_PATHS is TRUE.  See it for details.

   If DESCENDING_ORDER is true, send child messages in descending order.

   If REVPROPS is NULL, retrieve all revision properties; else, retrieve
   only the revision properties named by the (const char *) array elements
   (i.e. retrieve none if the array is empty).

   LOG_TARGET_HISTORY_AS_MERGEINFO, HANDLING_MERGED_REVISION, MOVE_BEHAVIOR,
   and NESTED_MERGES are as per the arguments of the same name to DO_LOGS.
   If HANDLING_MERGED_REVISION is true and *all* changed paths within REV are
   already represented in LOG_TARGET_HISTORY_AS_MERGEINFO, then don't send
   the log message for REV.  If SUBTRACTIVE_MERGE is true, then REV was
   reverse merged.

   If HANDLING_MERGED_REVISIONS is FALSE then ignore NESTED_MERGES.  Otherwise
   if NESTED_MERGES is not NULL and REV is contained in it, then don't send
   the log for REV, otherwise send it normally and add REV to
   NESTED_MERGES. */
static svn_error_t *
send_log(svn_revnum_t rev,
         svn_fs_t *fs,
         apr_hash_t *prefetched_changes,
         svn_mergeinfo_t log_target_history_as_mergeinfo,
         apr_hash_t *nested_merges,
         svn_boolean_t discover_changed_paths,
         svn_boolean_t subtractive_merge,
         svn_boolean_t handling_merged_revision,
         svn_move_behavior_t move_behavior,
         const apr_array_header_t *revprops,
         svn_boolean_t has_children,
         svn_log_entry_receiver_t receiver,
         void *receiver_baton,
         svn_repos_authz_func_t authz_read_func,
         void *authz_read_baton,
         apr_pool_t *pool)
{
  svn_log_entry_t *log_entry;
  /* Assume we want to send the log for REV. */
  svn_boolean_t found_rev_of_interest = TRUE;

  log_entry = svn_log_entry_create(pool);
  SVN_ERR(fill_log_entry(log_entry, rev, fs, prefetched_changes,
                         discover_changed_paths || handling_merged_revision,
                         move_behavior, revprops, 
                         authz_read_func, authz_read_baton, pool));
  log_entry->has_children = has_children;
  log_entry->subtractive_merge = subtractive_merge;

  /* Is REV a merged revision that is already part of
     LOG_TARGET_HISTORY_AS_MERGEINFO?  If so then there is no
     need to send it, since it already was (or will be) sent. */
  if (handling_merged_revision
      && log_entry->changed_paths2
      && log_target_history_as_mergeinfo
      && apr_hash_count(log_target_history_as_mergeinfo))
    {
      apr_hash_index_t *hi;
      apr_pool_t *iterpool = svn_pool_create(pool);

      /* REV was merged in, but it might already be part of the log target's
         natural history, so change our starting assumption. */
      found_rev_of_interest = FALSE;

      /* Look at each changed path in REV. */
      for (hi = apr_hash_first(pool, log_entry->changed_paths2);
           hi;
           hi = apr_hash_next(hi))
        {
          svn_boolean_t path_is_in_history = FALSE;
          const char *changed_path = svn__apr_hash_index_key(hi);
          apr_hash_index_t *hi2;

          /* Look at each path on the log target's mergeinfo. */
          for (hi2 = apr_hash_first(iterpool,
                                    log_target_history_as_mergeinfo);
               hi2;
               hi2 = apr_hash_next(hi2))
            {
              const char *mergeinfo_path = svn__apr_hash_index_key(hi2);
              svn_rangelist_t *rangelist = svn__apr_hash_index_val(hi2);

              /* Check whether CHANGED_PATH at revision REV is a child of
                 a (path, revision) tuple in LOG_TARGET_HISTORY_AS_MERGEINFO. */
              if (svn_fspath__skip_ancestor(mergeinfo_path, changed_path))
                {
                  int i;

                  for (i = 0; i < rangelist->nelts; i++)
                    {
                      svn_merge_range_t *range =
                        APR_ARRAY_IDX(rangelist, i,
                                      svn_merge_range_t *);
                      if (rev > range->start && rev <= range->end)
                        {
                          path_is_in_history = TRUE;
                          break;
                        }
                    }
                }
              if (path_is_in_history)
                break;
            }
          svn_pool_clear(iterpool);

          if (!path_is_in_history)
            {
              /* If even one path in LOG_ENTRY->CHANGED_PATHS2 is not part of
                 LOG_TARGET_HISTORY_AS_MERGEINFO, then we want to send the
                 log for REV. */
              found_rev_of_interest = TRUE;
              break;
            }
        }
      svn_pool_destroy(iterpool);
    }

  /* If we only got changed paths the sake of detecting redundant merged
     revisions, then be sure we don't send that info to the receiver. */
  if (!discover_changed_paths && handling_merged_revision)
    log_entry->changed_paths = log_entry->changed_paths2 = NULL;

  /* Send the entry to the receiver, unless it is a redundant merged
     revision. */
  if (found_rev_of_interest)
    {
      /* Is REV a merged revision we've already sent? */
      if (nested_merges && handling_merged_revision)
        {
          svn_revnum_t *merged_rev = apr_hash_get(nested_merges, &rev,
                                                  sizeof(svn_revnum_t *));

          if (merged_rev)
            {
              /* We already sent REV. */
              return SVN_NO_ERROR;
            }
          else
            {
              /* NESTED_REVS needs to last across all the send_log, do_logs,
                 handle_merged_revisions() recursions, so use the pool it
                 was created in at the top of the recursion. */
              apr_pool_t *hash_pool = apr_hash_pool_get(nested_merges);
              svn_revnum_t *long_lived_rev = apr_palloc(hash_pool,
                                                        sizeof(svn_revnum_t));
              *long_lived_rev = rev;
              apr_hash_set(nested_merges, long_lived_rev,
                           sizeof(svn_revnum_t *), long_lived_rev);
            }
        }

      return (*receiver)(receiver_baton, log_entry, pool);
    }
  else
    {
      return SVN_NO_ERROR;
    }
}

/* This controls how many history objects we keep open.  For any targets
   over this number we have to open and close their histories as needed,
   which is CPU intensive, but keeps us from using an unbounded amount of
   memory. */
#define MAX_OPEN_HISTORIES 32

/* Get the histories for PATHS, and store them in *HISTORIES.

   If IGNORE_MISSING_LOCATIONS is set, don't treat requests for bogus
   repository locations as fatal -- just ignore them.  */
static svn_error_t *
get_path_histories(apr_array_header_t **histories,
                   svn_fs_t *fs,
                   const apr_array_header_t *paths,
                   svn_revnum_t hist_start,
                   svn_revnum_t hist_end,
                   svn_boolean_t strict_node_history,
                   svn_boolean_t ignore_missing_locations,
                   svn_repos_authz_func_t authz_read_func,
                   void *authz_read_baton,
                   apr_pool_t *pool)
{
  svn_fs_root_t *root;
  apr_pool_t *iterpool;
  svn_error_t *err;
  int i;

  /* Create a history object for each path so we can walk through
     them all at the same time until we have all changes or LIMIT
     is reached.

     There is some pool fun going on due to the fact that we have
     to hold on to the old pool with the history before we can
     get the next history.
  */
  *histories = apr_array_make(pool, paths->nelts,
                              sizeof(struct path_info *));

  SVN_ERR(svn_fs_revision_root(&root, fs, hist_end, pool));

  iterpool = svn_pool_create(pool);
  for (i = 0; i < paths->nelts; i++)
    {
      const char *this_path = APR_ARRAY_IDX(paths, i, const char *);
      struct path_info *info = apr_palloc(pool,
                                          sizeof(struct path_info));

      if (authz_read_func)
        {
          svn_boolean_t readable;

          svn_pool_clear(iterpool);

          SVN_ERR(authz_read_func(&readable, root, this_path,
                                  authz_read_baton, iterpool));
          if (! readable)
            return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE, NULL, NULL);
        }

      info->path = svn_stringbuf_create(this_path, pool);
      info->done = FALSE;
      info->history_rev = hist_end;
      info->first_time = TRUE;

      if (i < MAX_OPEN_HISTORIES)
        {
          err = svn_fs_node_history(&info->hist, root, this_path, pool);
          if (err
              && ignore_missing_locations
              && (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
                  err->apr_err == SVN_ERR_FS_NOT_DIRECTORY ||
                  err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION))
            {
              svn_error_clear(err);
              continue;
            }
          SVN_ERR(err);
          info->newpool = svn_pool_create(pool);
          info->oldpool = svn_pool_create(pool);
        }
      else
        {
          info->hist = NULL;
          info->oldpool = NULL;
          info->newpool = NULL;
        }

      err = get_history(info, fs,
                        strict_node_history,
                        authz_read_func, authz_read_baton,
                        hist_start, pool);
      if (err
          && ignore_missing_locations
          && (err->apr_err == SVN_ERR_FS_NOT_FOUND ||
              err->apr_err == SVN_ERR_FS_NOT_DIRECTORY ||
              err->apr_err == SVN_ERR_FS_NO_SUCH_REVISION))
        {
          svn_error_clear(err);
          continue;
        }
      SVN_ERR(err);
      APR_ARRAY_PUSH(*histories, struct path_info *) = info;
    }
  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Remove and return the first item from ARR. */
static void *
array_pop_front(apr_array_header_t *arr)
{
  void *item = arr->elts;

  if (apr_is_empty_array(arr))
    return NULL;

  arr->elts += arr->elt_size;
  arr->nelts -= 1;
  arr->nalloc -= 1;
  return item;
}

/* A struct which represents a single revision range, and the paths which
   have mergeinfo in that range. */
struct path_list_range
{
  apr_array_header_t *paths;
  svn_merge_range_t range;

  /* Is RANGE the result of a reverse merge? */
  svn_boolean_t reverse_merge;
};

/* A struct which represents "inverse mergeinfo", that is, instead of having
   a path->revision_range_list mapping, which is the way mergeinfo is commonly
   represented, this struct enables a revision_range_list,path tuple, where
   the paths can be accessed by revision. */
struct rangelist_path
{
  svn_rangelist_t *rangelist;
  const char *path;
};

/* Comparator function for combine_mergeinfo_path_lists().  Sorts
   rangelist_path structs in increasing order based upon starting revision,
   then ending revision of the first element in the rangelist.

   This does not sort rangelists based upon subsequent elements, only the
   first range.  We'll sort any subsequent ranges in the correct order
   when they get bumped up to the front by removal of earlier ones, so we
   don't really have to sort them here.  See combine_mergeinfo_path_lists()
   for details. */
static int
compare_rangelist_paths(const void *a, const void *b)
{
  struct rangelist_path *rpa = *((struct rangelist_path *const *) a);
  struct rangelist_path *rpb = *((struct rangelist_path *const *) b);
  svn_merge_range_t *mra = APR_ARRAY_IDX(rpa->rangelist, 0,
                                         svn_merge_range_t *);
  svn_merge_range_t *mrb = APR_ARRAY_IDX(rpb->rangelist, 0,
                                         svn_merge_range_t *);

  if (mra->start < mrb->start)
    return -1;
  if (mra->start > mrb->start)
    return 1;
  if (mra->end < mrb->end)
    return -1;
  if (mra->end > mrb->end)
    return 1;

  return 0;
}

/* From MERGEINFO, return in *COMBINED_LIST, allocated in POOL, a list of
   'struct path_list_range's.  This list represents the rangelists in
   MERGEINFO and each path which has mergeinfo in that range.
   If REVERSE_MERGE is true, then MERGEINFO represents mergeinfo removed
   as the result of a reverse merge. */
static svn_error_t *
combine_mergeinfo_path_lists(apr_array_header_t **combined_list,
                             svn_mergeinfo_t mergeinfo,
                             svn_boolean_t reverse_merge,
                             apr_pool_t *pool)
{
  apr_hash_index_t *hi;
  apr_array_header_t *rangelist_paths;
  apr_pool_t *subpool = svn_pool_create(pool);

  /* Create a list of (revision range, path) tuples from MERGEINFO. */
  rangelist_paths = apr_array_make(subpool, apr_hash_count(mergeinfo),
                                   sizeof(struct rangelist_path *));
  for (hi = apr_hash_first(subpool, mergeinfo); hi;
       hi = apr_hash_next(hi))
    {
      int i;
      struct rangelist_path *rp = apr_palloc(subpool, sizeof(*rp));

      rp->path = svn__apr_hash_index_key(hi);
      rp->rangelist = svn__apr_hash_index_val(hi);
      APR_ARRAY_PUSH(rangelist_paths, struct rangelist_path *) = rp;

      /* We need to make local copies of the rangelist, since we will be
         modifying it, below. */
      rp->rangelist = svn_rangelist_dup(rp->rangelist, subpool);

      /* Make all of the rangelists inclusive, both start and end. */
      for (i = 0; i < rp->rangelist->nelts; i++)
        APR_ARRAY_IDX(rp->rangelist, i, svn_merge_range_t *)->start += 1;
    }

  /* Loop over the (revision range, path) tuples, chopping them into
     (revision range, paths) tuples, and appending those to the output
     list. */
  if (! *combined_list)
    *combined_list = apr_array_make(pool, 0, sizeof(struct path_list_range *));

  while (rangelist_paths->nelts > 1)
    {
      svn_revnum_t youngest, next_youngest, tail, youngest_end;
      struct path_list_range *plr;
      struct rangelist_path *rp;
      int num_revs;
      int i;

      /* First, sort the list such that the start revision of the first
         revision arrays are sorted. */
      svn_sort__array(rangelist_paths, compare_rangelist_paths);

      /* Next, find the number of revision ranges which start with the same
         revision. */
      rp = APR_ARRAY_IDX(rangelist_paths, 0, struct rangelist_path *);
      youngest =
        APR_ARRAY_IDX(rp->rangelist, 0, struct svn_merge_range_t *)->start;
      next_youngest = youngest;
      for (num_revs = 1; next_youngest == youngest; num_revs++)
        {
          if (num_revs == rangelist_paths->nelts)
            {
              num_revs += 1;
              break;
            }
          rp = APR_ARRAY_IDX(rangelist_paths, num_revs,
                             struct rangelist_path *);
          next_youngest = APR_ARRAY_IDX(rp->rangelist, 0,
                                        struct svn_merge_range_t *)->start;
        }
      num_revs -= 1;

      /* The start of the new range will be YOUNGEST, and we now find the end
         of the new range, which should be either one less than the next
         earliest start of a rangelist, or the end of the first rangelist. */
      youngest_end =
        APR_ARRAY_IDX(APR_ARRAY_IDX(rangelist_paths, 0,
                                    struct rangelist_path *)->rangelist,
                      0, svn_merge_range_t *)->end;
      if ( (next_youngest == youngest) || (youngest_end < next_youngest) )
        tail = youngest_end;
      else
        tail = next_youngest - 1;

      /* Insert the (earliest, tail) tuple into the output list, along with
         a list of paths which match it. */
      plr = apr_palloc(pool, sizeof(*plr));
      plr->reverse_merge = reverse_merge;
      plr->range.start = youngest;
      plr->range.end = tail;
      plr->paths = apr_array_make(pool, num_revs, sizeof(const char *));
      for (i = 0; i < num_revs; i++)
        APR_ARRAY_PUSH(plr->paths, const char *) =
          APR_ARRAY_IDX(rangelist_paths, i, struct rangelist_path *)->path;
      APR_ARRAY_PUSH(*combined_list, struct path_list_range *) = plr;

      /* Now, check to see which (rangelist path) combinations we can remove,
         and do so. */
      for (i = 0; i < num_revs; i++)
        {
          svn_merge_range_t *range;
          rp = APR_ARRAY_IDX(rangelist_paths, i, struct rangelist_path *);
          range = APR_ARRAY_IDX(rp->rangelist, 0, svn_merge_range_t *);

          /* Set the start of the range to beyond the end of the range we
             just built.  If the range is now "inverted", we can get pop it
             off the list. */
          range->start = tail + 1;
          if (range->start > range->end)
            {
              if (rp->rangelist->nelts == 1)
                {
                  /* The range is the only on its list, so we should remove
                     the entire rangelist_path, adjusting our loop control
                     variables appropriately. */
                  array_pop_front(rangelist_paths);
                  i--;
                  num_revs--;
                }
              else
                {
                  /* We have more than one range on the list, so just remove
                     the first one. */
                  array_pop_front(rp->rangelist);
                }
            }
        }
    }

  /* Finally, add the last remaining (revision range, path) to the output
     list. */
  if (rangelist_paths->nelts > 0)
    {
      struct rangelist_path *first_rp =
        APR_ARRAY_IDX(rangelist_paths, 0, struct rangelist_path *);
      while (first_rp->rangelist->nelts > 0)
        {
          struct path_list_range *plr = apr_palloc(pool, sizeof(*plr));

          plr->reverse_merge = reverse_merge;
          plr->paths = apr_array_make(pool, 1, sizeof(const char *));
          APR_ARRAY_PUSH(plr->paths, const char *) = first_rp->path;
          plr->range = *APR_ARRAY_IDX(first_rp->rangelist, 0,
                                      svn_merge_range_t *);
          array_pop_front(first_rp->rangelist);
          APR_ARRAY_PUSH(*combined_list, struct path_list_range *) = plr;
        }
    }

  svn_pool_destroy(subpool);

  return SVN_NO_ERROR;
}


/* Pity that C is so ... linear. */
static svn_error_t *
do_logs(svn_fs_t *fs,
        const apr_array_header_t *paths,
        svn_mergeinfo_t log_target_history_as_mergeinfo,
        svn_mergeinfo_t processed,
        apr_hash_t *nested_merges,
        svn_revnum_t hist_start,
        svn_revnum_t hist_end,
        int limit,
        svn_boolean_t discover_changed_paths,
        svn_boolean_t strict_node_history,
        svn_boolean_t include_merged_revisions,
        svn_boolean_t handling_merged_revisions,
        svn_boolean_t subtractive_merge,
        svn_boolean_t ignore_missing_locations,
        svn_move_behavior_t move_behavior,
        const apr_array_header_t *revprops,
        svn_boolean_t descending_order,
        svn_log_entry_receiver_t receiver,
        void *receiver_baton,
        svn_repos_authz_func_t authz_read_func,
        void *authz_read_baton,
        apr_pool_t *pool);

/* Comparator function for handle_merged_revisions().  Sorts path_list_range
   structs in increasing order based on the struct's RANGE.START revision,
   then RANGE.END revision. */
static int
compare_path_list_range(const void *a, const void *b)
{
  struct path_list_range *plr_a = *((struct path_list_range *const *) a);
  struct path_list_range *plr_b = *((struct path_list_range *const *) b);

  if (plr_a->range.start < plr_b->range.start)
    return -1;
  if (plr_a->range.start > plr_b->range.start)
    return 1;
  if (plr_a->range.end < plr_b->range.end)
    return -1;
  if (plr_a->range.end > plr_b->range.end)
    return 1;

  return 0;
}

/* Examine the ADDED_MERGEINFO and DELETED_MERGEINFO for revision REV in FS
   (as collected by examining paths of interest to a log operation), and
   determine which revisions to report as having been merged or reverse-merged
   via the commit resulting in REV.

   Silently ignore some failures to find the revisions mentioned in the
   added/deleted mergeinfos, as might happen if there is invalid mergeinfo.

   Other parameters are as described by do_logs(), around which this
   is a recursion wrapper. */
static svn_error_t *
handle_merged_revisions(svn_revnum_t rev,
                        svn_fs_t *fs,
                        svn_mergeinfo_t log_target_history_as_mergeinfo,
                        apr_hash_t *nested_merges,
                        svn_mergeinfo_t processed,
                        svn_mergeinfo_t added_mergeinfo,
                        svn_mergeinfo_t deleted_mergeinfo,
                        svn_boolean_t discover_changed_paths,
                        svn_boolean_t strict_node_history,
                        svn_move_behavior_t move_behavior,
                        const apr_array_header_t *revprops,
                        svn_log_entry_receiver_t receiver,
                        void *receiver_baton,
                        svn_repos_authz_func_t authz_read_func,
                        void *authz_read_baton,
                        apr_pool_t *pool)
{
  apr_array_header_t *combined_list = NULL;
  svn_log_entry_t *empty_log_entry;
  apr_pool_t *iterpool;
  int i;

  if (apr_hash_count(added_mergeinfo) == 0
      && apr_hash_count(deleted_mergeinfo) == 0)
    return SVN_NO_ERROR;

  if (apr_hash_count(added_mergeinfo))
    SVN_ERR(combine_mergeinfo_path_lists(&combined_list, added_mergeinfo,
                                          FALSE, pool));

  if (apr_hash_count(deleted_mergeinfo))
    SVN_ERR(combine_mergeinfo_path_lists(&combined_list, deleted_mergeinfo,
                                          TRUE, pool));

  SVN_ERR_ASSERT(combined_list != NULL);
  svn_sort__array(combined_list, compare_path_list_range);

  /* Because the combined_lists are ordered youngest to oldest,
     iterate over them in reverse. */
  iterpool = svn_pool_create(pool);
  for (i = combined_list->nelts - 1; i >= 0; i--)
    {
      struct path_list_range *pl_range
        = APR_ARRAY_IDX(combined_list, i, struct path_list_range *);

      svn_pool_clear(iterpool);
      SVN_ERR(do_logs(fs, pl_range->paths, log_target_history_as_mergeinfo,
                      processed, nested_merges,
                      pl_range->range.start, pl_range->range.end, 0,
                      discover_changed_paths, strict_node_history,
                      TRUE, pl_range->reverse_merge, TRUE, TRUE,
                      move_behavior, revprops, TRUE, receiver, receiver_baton,
                      authz_read_func, authz_read_baton, iterpool));
    }
  svn_pool_destroy(iterpool);

  /* Send the empty revision.  */
  empty_log_entry = svn_log_entry_create(pool);
  empty_log_entry->revision = SVN_INVALID_REVNUM;
  return (*receiver)(receiver_baton, empty_log_entry, pool);
}

/* This is used by do_logs to differentiate between forward and
   reverse merges. */
struct added_deleted_mergeinfo
{
  svn_mergeinfo_t added_mergeinfo;
  svn_mergeinfo_t deleted_mergeinfo;
};

/* Reduce the search range PATHS, HIST_START, HIST_END by removing
   parts already covered by PROCESSED.  If reduction is possible
   elements may be removed from PATHS and *START_REDUCED and
   *END_REDUCED may be set to a narrower range. */
static svn_error_t *
reduce_search(apr_array_header_t *paths,
              svn_revnum_t *hist_start,
              svn_revnum_t *hist_end,
              svn_mergeinfo_t processed,
              apr_pool_t *scratch_pool)
{
  /* We add 1 to end to compensate for store_search */
  svn_revnum_t start = *hist_start <= *hist_end ? *hist_start : *hist_end;
  svn_revnum_t end = *hist_start <= *hist_end ? *hist_end + 1 : *hist_start + 1;
  int i;

  for (i = 0; i < paths->nelts; ++i)
    {
      const char *path = APR_ARRAY_IDX(paths, i, const char *);
      svn_rangelist_t *ranges = svn_hash_gets(processed, path);
      int j;

      if (!ranges)
        continue;

      /* ranges is ordered, could we use some sort of binary search
         rather than iterating? */
      for (j = 0; j < ranges->nelts; ++j)
        {
          svn_merge_range_t *range = APR_ARRAY_IDX(ranges, j,
                                                   svn_merge_range_t *);
          if (range->start <= start && range->end >= end)
            {
              for (j = i; j < paths->nelts - 1; ++j)
                APR_ARRAY_IDX(paths, j, const char *)
                  = APR_ARRAY_IDX(paths, j + 1, const char *);

              --paths->nelts;
              --i;
              break;
            }

          /* If there is only one path then we also check for a
             partial overlap rather than the full overlap above, and
             reduce the [hist_start, hist_end] range rather than
             dropping the path. */
          if (paths->nelts == 1)
            {
              if (range->start <= start && range->end > start)
                {
                  if (start == *hist_start)
                    *hist_start = range->end - 1;
                  else
                    *hist_end = range->end - 1;
                  break;
                }
              if (range->start < end && range->end >= end)
                {
                  if (start == *hist_start)
                    *hist_end = range->start;
                  else
                    *hist_start = range->start;
                  break;
                }
            }
        }
    }

  return SVN_NO_ERROR;
}

/* Extend PROCESSED to cover PATHS from HIST_START to HIST_END */
static svn_error_t *
store_search(svn_mergeinfo_t processed,
             const apr_array_header_t *paths,
             svn_revnum_t hist_start,
             svn_revnum_t hist_end,
             apr_pool_t *scratch_pool)
{
  /* We add 1 to end so that we can use the mergeinfo API to handle
     singe revisions where HIST_START is equal to HIST_END. */
  svn_revnum_t start = hist_start <= hist_end ? hist_start : hist_end;
  svn_revnum_t end = hist_start <= hist_end ? hist_end + 1 : hist_start + 1;
  svn_mergeinfo_t mergeinfo = svn_hash__make(scratch_pool);
  apr_pool_t *processed_pool = apr_hash_pool_get(processed);
  int i;

  for (i = 0; i < paths->nelts; ++i)
    {
      const char *path = APR_ARRAY_IDX(paths, i, const char *);
      svn_rangelist_t *ranges = apr_array_make(processed_pool, 1,
                                               sizeof(svn_merge_range_t*));
      svn_merge_range_t *range = apr_palloc(processed_pool,
                                            sizeof(svn_merge_range_t));

      range->start = start;
      range->end = end;
      range->inheritable = TRUE;
      APR_ARRAY_PUSH(ranges, svn_merge_range_t *) = range;
      svn_hash_sets(mergeinfo, apr_pstrdup(processed_pool, path), ranges);
    }
  SVN_ERR(svn_mergeinfo_merge2(processed, mergeinfo,
                               apr_hash_pool_get(processed), scratch_pool));

  return SVN_NO_ERROR;
}

/* Find logs for PATHS from HIST_START to HIST_END in FS, and invoke
   RECEIVER with RECEIVER_BATON on them.  If DESCENDING_ORDER is TRUE, send
   the logs back as we find them, else buffer the logs and send them back
   in youngest->oldest order.

   If IGNORE_MISSING_LOCATIONS is set, don't treat requests for bogus
   repository locations as fatal -- just ignore them.

   MOVE_BEHAVIOR is a simple pass-through parameter that tells the FS
   layer which changes to report as moves instead of additions.

   If LOG_TARGET_HISTORY_AS_MERGEINFO is not NULL then it contains mergeinfo
   representing the history of PATHS between HIST_START and HIST_END.

   If HANDLING_MERGED_REVISIONS is TRUE then this is a recursive call for
   merged revisions, see INCLUDE_MERGED_REVISIONS argument to
   svn_repos_get_logs4().  If SUBTRACTIVE_MERGE is true, then this is a
   recursive call for reverse merged revisions.

   If NESTED_MERGES is not NULL then it is a hash of revisions (svn_revnum_t *
   mapped to svn_revnum_t *) for logs that were previously sent.  On the first
   call to do_logs it should always be NULL.  If INCLUDE_MERGED_REVISIONS is
   TRUE, then NESTED_MERGES will be created on the first call to do_logs,
   allocated in POOL.  It is then shared across
   do_logs()/send_logs()/handle_merge_revisions() recursions, see also the
   argument of the same name in send_logs().

   PROCESSED is a mergeinfo hash that represents the paths and
   revisions that have already been searched.  Allocated like
   NESTED_MERGES above.

   All other parameters are the same as svn_repos_get_logs4().
 */
static svn_error_t *
do_logs(svn_fs_t *fs,
        const apr_array_header_t *paths,
        svn_mergeinfo_t log_target_history_as_mergeinfo,
        svn_mergeinfo_t processed,
        apr_hash_t *nested_merges,
        svn_revnum_t hist_start,
        svn_revnum_t hist_end,
        int limit,
        svn_boolean_t discover_changed_paths,
        svn_boolean_t strict_node_history,
        svn_boolean_t include_merged_revisions,
        svn_boolean_t subtractive_merge,
        svn_boolean_t handling_merged_revisions,
        svn_boolean_t ignore_missing_locations,
        svn_move_behavior_t move_behavior,
        const apr_array_header_t *revprops,
        svn_boolean_t descending_order,
        svn_log_entry_receiver_t receiver,
        void *receiver_baton,
        svn_repos_authz_func_t authz_read_func,
        void *authz_read_baton,
        apr_pool_t *pool)
{
  apr_pool_t *iterpool;
  apr_pool_t *subpool = NULL;
  apr_array_header_t *revs = NULL;
  apr_hash_t *rev_mergeinfo = NULL;
  svn_revnum_t current;
  apr_array_header_t *histories;
  svn_boolean_t any_histories_left = TRUE;
  int send_count = 0;
  int i;

  if (processed)
    {
      /* Casting away const. This only happens on recursive calls when
         it is known to be safe because we allocated paths. */
      SVN_ERR(reduce_search((apr_array_header_t *)paths, &hist_start, &hist_end,
                            processed, pool));
    }

  if (!paths->nelts)
    return SVN_NO_ERROR;

  if (processed)
    SVN_ERR(store_search(processed, paths, hist_start, hist_end, pool));

  /* We have a list of paths and a revision range.  But we don't care
     about all the revisions in the range -- only the ones in which
     one of our paths was changed.  So let's go figure out which
     revisions contain real changes to at least one of our paths.  */
  SVN_ERR(get_path_histories(&histories, fs, paths, hist_start, hist_end,
                             strict_node_history, ignore_missing_locations,
                             authz_read_func, authz_read_baton, pool));

  /* Loop through all the revisions in the range and add any
     where a path was changed to the array, or if they wanted
     history in reverse order just send it to them right away. */
  iterpool = svn_pool_create(pool);
  for (current = hist_end;
       any_histories_left;
       current = next_history_rev(histories))
    {
      svn_boolean_t changed = FALSE;
      any_histories_left = FALSE;
      svn_pool_clear(iterpool);

      for (i = 0; i < histories->nelts; i++)
        {
          struct path_info *info = APR_ARRAY_IDX(histories, i,
                                                 struct path_info *);

          /* Check history for this path in current rev. */
          SVN_ERR(check_history(&changed, info, fs, current,
                                strict_node_history, authz_read_func,
                                authz_read_baton, hist_start, pool));
          if (! info->done)
            any_histories_left = TRUE;
        }

      /* If any of the paths changed in this rev then add or send it. */
      if (changed)
        {
          svn_mergeinfo_t added_mergeinfo = NULL;
          svn_mergeinfo_t deleted_mergeinfo = NULL;
          svn_boolean_t has_children = FALSE;
          apr_hash_t *changes = NULL;

          /* If we're including merged revisions, we need to calculate
             the mergeinfo deltas committed in this revision to our
             various paths. */
          if (include_merged_revisions)
            {
              apr_array_header_t *cur_paths =
                apr_array_make(iterpool, paths->nelts, sizeof(const char *));

              /* Get the current paths of our history objects so we can
                 query mergeinfo. */
              /* ### TODO: Should this be ignoring depleted history items? */
              for (i = 0; i < histories->nelts; i++)
                {
                  struct path_info *info = APR_ARRAY_IDX(histories, i,
                                                         struct path_info *);
                  APR_ARRAY_PUSH(cur_paths, const char *) = info->path->data;
                }
              SVN_ERR(get_combined_mergeinfo_changes(&added_mergeinfo,
                                                     &deleted_mergeinfo,
                                                     &changes,
                                                     fs, cur_paths,
                                                     current,
                                                     iterpool, iterpool));
              has_children = (apr_hash_count(added_mergeinfo) > 0
                              || apr_hash_count(deleted_mergeinfo) > 0);
            }

          /* If our caller wants logs in descending order, we can send
             'em now (because that's the order we're crawling history
             in anyway). */
          if (descending_order)
            {
              SVN_ERR(send_log(current, fs, changes,
                               log_target_history_as_mergeinfo, nested_merges,
                               discover_changed_paths,
                               subtractive_merge, handling_merged_revisions,
                               move_behavior, revprops, has_children,
                               receiver, receiver_baton,
                               authz_read_func, authz_read_baton, iterpool));

              if (has_children) /* Implies include_merged_revisions == TRUE */
                {
                  if (!nested_merges)
                    {
                      /* We're at the start of the recursion stack, create a
                         single hash to be shared across all of the merged
                         recursions so we can track and squelch duplicates. */
                      subpool = svn_pool_create(pool);
                      nested_merges = svn_hash__make(subpool);
                      processed = svn_hash__make(subpool);
                    }

                  SVN_ERR(handle_merged_revisions(
                    current, fs,
                    log_target_history_as_mergeinfo, nested_merges,
                    processed,
                    added_mergeinfo, deleted_mergeinfo,
                    discover_changed_paths,
                    strict_node_history,
                    move_behavior,
                    revprops,
                    receiver, receiver_baton,
                    authz_read_func,
                    authz_read_baton,
                    iterpool));
                }
              if (limit && ++send_count >= limit)
                break;
            }
          /* Otherwise, the caller wanted logs in ascending order, so
             we have to buffer up a list of revs and (if doing
             mergeinfo) a hash of related mergeinfo deltas, and
             process them later. */
          else
            {
              if (! revs)
                revs = apr_array_make(pool, 64, sizeof(svn_revnum_t));
              APR_ARRAY_PUSH(revs, svn_revnum_t) = current;

              if (added_mergeinfo || deleted_mergeinfo)
                {
                  svn_revnum_t *cur_rev =
                    apr_pmemdup(pool, &current, sizeof(cur_rev));
                  struct added_deleted_mergeinfo *add_and_del_mergeinfo =
                    apr_palloc(pool, sizeof(*add_and_del_mergeinfo));

                  /* If we have added or deleted mergeinfo, both are non-null */
                  SVN_ERR_ASSERT(added_mergeinfo && deleted_mergeinfo);
                  add_and_del_mergeinfo->added_mergeinfo =
                    svn_mergeinfo_dup(added_mergeinfo, pool);
                  add_and_del_mergeinfo->deleted_mergeinfo =
                    svn_mergeinfo_dup(deleted_mergeinfo, pool);

                  if (! rev_mergeinfo)
                    rev_mergeinfo = svn_hash__make(pool);
                  apr_hash_set(rev_mergeinfo, cur_rev, sizeof(*cur_rev),
                               add_and_del_mergeinfo);
                }
            }
        }
    }
  svn_pool_destroy(iterpool);

  if (subpool)
    {
      nested_merges = NULL;
      svn_pool_destroy(subpool);
    }

  if (revs)
    {
      /* Work loop for processing the revisions we found since they wanted
         history in forward order. */
      iterpool = svn_pool_create(pool);
      for (i = 0; i < revs->nelts; ++i)
        {
          svn_mergeinfo_t added_mergeinfo;
          svn_mergeinfo_t deleted_mergeinfo;
          svn_boolean_t has_children = FALSE;

          svn_pool_clear(iterpool);
          current = APR_ARRAY_IDX(revs, revs->nelts - i - 1, svn_revnum_t);

          /* If we've got a hash of revision mergeinfo (which can only
             happen if INCLUDE_MERGED_REVISIONS was set), we check to
             see if this revision is one which merged in other
             revisions we need to handle recursively. */
          if (rev_mergeinfo)
            {
              struct added_deleted_mergeinfo *add_and_del_mergeinfo =
                apr_hash_get(rev_mergeinfo, &current, sizeof(svn_revnum_t));
              added_mergeinfo = add_and_del_mergeinfo->added_mergeinfo;
              deleted_mergeinfo = add_and_del_mergeinfo->deleted_mergeinfo;
              has_children = (apr_hash_count(added_mergeinfo) > 0
                              || apr_hash_count(deleted_mergeinfo) > 0);
            }

          SVN_ERR(send_log(current, fs, NULL,
                           log_target_history_as_mergeinfo, nested_merges,
                           discover_changed_paths, subtractive_merge,
                           handling_merged_revisions, move_behavior,
                           revprops, has_children,
                           receiver, receiver_baton, authz_read_func,
                           authz_read_baton, iterpool));
          if (has_children)
            {
              if (!nested_merges)
                {
                  subpool = svn_pool_create(pool);
                  nested_merges = svn_hash__make(subpool);
                }

              SVN_ERR(handle_merged_revisions(current, fs,
                                              log_target_history_as_mergeinfo,
                                              nested_merges,
                                              processed,
                                              added_mergeinfo,
                                              deleted_mergeinfo,
                                              discover_changed_paths,
                                              strict_node_history,
                                              move_behavior, revprops,
                                              receiver, receiver_baton,
                                              authz_read_func,
                                              authz_read_baton,
                                              iterpool));
            }
          if (limit && i + 1 >= limit)
            break;
        }
      svn_pool_destroy(iterpool);
    }

  return SVN_NO_ERROR;
}

struct location_segment_baton
{
  apr_array_header_t *history_segments;
  apr_pool_t *pool;
};

/* svn_location_segment_receiver_t implementation for svn_repos_get_logs4. */
static svn_error_t *
location_segment_receiver(svn_location_segment_t *segment,
                          void *baton,
                          apr_pool_t *pool)
{
  struct location_segment_baton *b = baton;

  APR_ARRAY_PUSH(b->history_segments, svn_location_segment_t *) =
    svn_location_segment_dup(segment, b->pool);

  return SVN_NO_ERROR;
}


/* Populate *PATHS_HISTORY_MERGEINFO with mergeinfo representing the combined
   history of each path in PATHS between START_REV and END_REV in REPOS's
   filesystem.  START_REV and END_REV must be valid revisions.  RESULT_POOL
   is used to allocate *PATHS_HISTORY_MERGEINFO, SCRATCH_POOL is used for all
   other (temporary) allocations.  Other parameters are the same as
   svn_repos_get_logs4(). */
static svn_error_t *
get_paths_history_as_mergeinfo(svn_mergeinfo_t *paths_history_mergeinfo,
                               svn_repos_t *repos,
                               const apr_array_header_t *paths,
                               svn_revnum_t start_rev,
                               svn_revnum_t end_rev,
                               svn_repos_authz_func_t authz_read_func,
                               void *authz_read_baton,
                               apr_pool_t *result_pool,
                               apr_pool_t *scratch_pool)
{
  int i;
  svn_mergeinfo_t path_history_mergeinfo;
  apr_pool_t *iterpool = svn_pool_create(scratch_pool);

  SVN_ERR_ASSERT(SVN_IS_VALID_REVNUM(start_rev));
  SVN_ERR_ASSERT(SVN_IS_VALID_REVNUM(end_rev));

  /* Ensure START_REV is the youngest revision, as required by
     svn_repos_node_location_segments, for which this is an iterative
     wrapper. */
  if (start_rev < end_rev)
    {
      svn_revnum_t tmp_rev = start_rev;
      start_rev = end_rev;
      end_rev = tmp_rev;
    }

  *paths_history_mergeinfo = svn_hash__make(result_pool);

  for (i = 0; i < paths->nelts; i++)
    {
      const char *this_path = APR_ARRAY_IDX(paths, i, const char *);
      struct location_segment_baton loc_seg_baton;

      svn_pool_clear(iterpool);
      loc_seg_baton.pool = scratch_pool;
      loc_seg_baton.history_segments =
        apr_array_make(iterpool, 4, sizeof(svn_location_segment_t *));

      SVN_ERR(svn_repos_node_location_segments(repos, this_path, start_rev,
                                               start_rev, end_rev,
                                               location_segment_receiver,
                                               &loc_seg_baton,
                                               authz_read_func,
                                               authz_read_baton,
                                               iterpool));

      SVN_ERR(svn_mergeinfo__mergeinfo_from_segments(
        &path_history_mergeinfo, loc_seg_baton.history_segments, iterpool));
      SVN_ERR(svn_mergeinfo_merge2(*paths_history_mergeinfo,
                                   svn_mergeinfo_dup(path_history_mergeinfo,
                                                     result_pool),
                                   result_pool, iterpool));
    }
  svn_pool_destroy(iterpool);
  return SVN_NO_ERROR;
}

svn_error_t *
svn_repos_get_logs5(svn_repos_t *repos,
                    const apr_array_header_t *paths,
                    svn_revnum_t start,
                    svn_revnum_t end,
                    int limit,
                    svn_boolean_t discover_changed_paths,
                    svn_boolean_t strict_node_history,
                    svn_boolean_t include_merged_revisions,
                    svn_move_behavior_t move_behavior,
                    const apr_array_header_t *revprops,
                    svn_repos_authz_func_t authz_read_func,
                    void *authz_read_baton,
                    svn_log_entry_receiver_t receiver,
                    void *receiver_baton,
                    apr_pool_t *pool)
{
  svn_revnum_t head = SVN_INVALID_REVNUM;
  svn_fs_t *fs = repos->fs;
  svn_boolean_t descending_order;
  svn_mergeinfo_t paths_history_mergeinfo = NULL;

  if (revprops)
    {
      int i;
      apr_array_header_t *new_revprops
        = apr_array_make(pool, revprops->nelts, sizeof(svn_string_t *));

      for (i = 0; i < revprops->nelts; ++i)
        APR_ARRAY_PUSH(new_revprops, svn_string_t *)
          = svn_string_create(APR_ARRAY_IDX(revprops, i, const char *), pool);

      revprops = new_revprops;
    }
  
  /* Setup log range. */
  SVN_ERR(svn_fs_youngest_rev(&head, fs, pool));

  if (! SVN_IS_VALID_REVNUM(start))
    start = head;

  if (! SVN_IS_VALID_REVNUM(end))
    end = head;

  /* Check that revisions are sane before ever invoking receiver. */
  if (start > head)
    return svn_error_createf
      (SVN_ERR_FS_NO_SUCH_REVISION, 0,
       _("No such revision %ld"), start);
  if (end > head)
    return svn_error_createf
      (SVN_ERR_FS_NO_SUCH_REVISION, 0,
       _("No such revision %ld"), end);

  /* Ensure a youngest-to-oldest revision crawl ordering using our
     (possibly sanitized) range values. */
  descending_order = start >= end;
  if (descending_order)
    {
      svn_revnum_t tmp_rev = start;
      start = end;
      end = tmp_rev;
    }

  if (! paths)
    paths = apr_array_make(pool, 0, sizeof(const char *));

  /* If we're not including merged revisions, and we were given no
     paths or a single empty (or "/") path, then we can bypass a bunch
     of complexity because we already know in which revisions the root
     directory was changed -- all of them.  */
  if ((! include_merged_revisions)
      && ((! paths->nelts)
          || ((paths->nelts == 1)
              && (svn_path_is_empty(APR_ARRAY_IDX(paths, 0, const char *))
                  || (strcmp(APR_ARRAY_IDX(paths, 0, const char *),
                             "/") == 0)))))
    {
      apr_uint64_t send_count = 0;
      int i;
      apr_pool_t *iterpool = svn_pool_create(pool);

      /* If we are provided an authz callback function, use it to
         verify that the user has read access to the root path in the
         first of our revisions.

         ### FIXME:  Strictly speaking, we should be checking this
         ### access in every revision along the line.  But currently,
         ### there are no known authz implementations which concern
         ### themselves with per-revision access.  */
      if (authz_read_func)
        {
          svn_boolean_t readable;
          svn_fs_root_t *rev_root;

          SVN_ERR(svn_fs_revision_root(&rev_root, fs,
                                       descending_order ? end : start, pool));
          SVN_ERR(authz_read_func(&readable, rev_root, "",
                                  authz_read_baton, pool));
          if (! readable)
            return svn_error_create(SVN_ERR_AUTHZ_UNREADABLE, NULL, NULL);
        }

      send_count = end - start + 1;
      if (limit > 0 && send_count > limit)
        send_count = limit;
      for (i = 0; i < send_count; ++i)
        {
          svn_revnum_t rev;

          svn_pool_clear(iterpool);

          if (descending_order)
            rev = end - i;
          else
            rev = start + i;
          SVN_ERR(send_log(rev, fs, NULL, NULL, NULL,
                           discover_changed_paths, FALSE,
                           FALSE, move_behavior, revprops, FALSE, receiver,
                           receiver_baton, authz_read_func,
                           authz_read_baton, iterpool));
        }
      svn_pool_destroy(iterpool);

      return SVN_NO_ERROR;
    }

  /* If we are including merged revisions, then create mergeinfo that
     represents all of PATHS' history between START and END.  We will use
     this later to squelch duplicate log revisions that might exist in
     both natural history and merged-in history.  See
     http://subversion.tigris.org/issues/show_bug.cgi?id=3650#desc5 */
  if (include_merged_revisions)
    {
      apr_pool_t *subpool = svn_pool_create(pool);

      SVN_ERR(get_paths_history_as_mergeinfo(&paths_history_mergeinfo,
                                             repos, paths, start, end,
                                             authz_read_func,
                                             authz_read_baton,
                                             pool, subpool));
      svn_pool_destroy(subpool);
    }

  return do_logs(repos->fs, paths, paths_history_mergeinfo, NULL, NULL, start, end,
                 limit, discover_changed_paths, strict_node_history,
                 include_merged_revisions, FALSE, FALSE, FALSE,
                 move_behavior, revprops,
                 descending_order, receiver, receiver_baton,
                 authz_read_func, authz_read_baton, pool);
}
