#include "private/svn_diff_private.h"
/* Adjust *PATH, *ORIG_PATH_1 and *ORIG_PATH_2, representing the changed file
                             const char *copyfrom_path,
                             svn_revnum_t copyfrom_rev,
                             const char *path,
  if (copyfrom_rev != SVN_INVALID_REVNUM)
    SVN_ERR(svn_stream_printf_from_utf8(os, header_encoding, result_pool,
                                        "copy from %s@%ld%s", copyfrom_path,
                                        copyfrom_rev, APR_EOL_STR));
  else
    SVN_ERR(svn_stream_printf_from_utf8(os, header_encoding, result_pool,
                                        "copy from %s%s", copyfrom_path,
                                        APR_EOL_STR));
 * revisions being diffed. COPYFROM_PATH and COPYFROM_REV indicate where the
 * diffed item was copied from.
                      svn_revnum_t copyfrom_rev,
                                           copyfrom_path, copyfrom_rev,
                                           repos_relpath2,
   to OUTSTREAM.   Of course, OUTSTREAM will probably be whatever was
   passed to svn_client_diff6(), which is probably stdout.
                   svn_stream_t *outstream,
  const char *path1 = orig_path1;
  const char *path2 = orig_path2;
      const char *adjusted_path1 = path1;
      const char *adjusted_path2 = path2;
      SVN_ERR(svn_stream_printf_from_utf8(outstream, encoding, pool,
                                          "Index: %s" APR_EOL_STR
                                          SVN_DIFF__EQUAL_STRING APR_EOL_STR,
                                          path));
        SVN_ERR(print_git_diff_header(outstream, &label1, &label2,
                                      svn_diff_op_modified,
                                      path1, path2, rev1, rev2, NULL,
                                      SVN_INVALID_REVNUM,
                                      encoding, pool));

      /* --- label1
       * +++ label2 */
      SVN_ERR(svn_diff__unidiff_write_header(
        outstream, encoding, label1, label2, pool));
  SVN_ERR(svn_stream_printf_from_utf8(outstream, encoding, pool,
                                      _("%sProperty changes on: %s%s"),
                                      APR_EOL_STR,
                                      use_git_diff_format ? path1 : path,
                                      APR_EOL_STR));
  SVN_ERR(svn_stream_printf_from_utf8(outstream, encoding, pool,
                                      SVN_DIFF__UNDER_STRING APR_EOL_STR));
  SVN_ERR(svn_diff__display_prop_diffs(
            outstream, encoding, propchanges, original_props,
            TRUE /* pretty_print_mergeinfo */, pool));
  svn_stream_t *outstream;
  svn_stream_t *errstream;
     svn_client_diff6(), either may be SVN_INVALID_REVNUM.  We need these
  /* Whether property differences are ignored. */
  svn_boolean_t ignore_properties;

  /* Whether to show only property changes. */
  svn_boolean_t properties_only;

  /* Whether deletion of a file is summarized versus showing a full diff. */
  svn_boolean_t no_diff_deleted;

  /* Whether the local diff target of a repos->wc diff is a copy. */
  svn_boolean_t repos_wc_diff_target_is_copy;

                   struct diff_cmd_baton *diff_cmd_baton,
  /* If property differences are ignored, there's nothing to do. */
  if (diff_cmd_baton->ignore_properties)
    return SVN_NO_ERROR;

                                 diff_cmd_baton->outstream,
                                            diff_cmd_baton,
                     svn_revnum_t copyfrom_rev,
                     struct diff_cmd_baton *diff_cmd_baton)
  svn_stream_t *errstream = diff_cmd_baton->errstream;
  svn_stream_t *outstream = diff_cmd_baton->outstream;
  const char *path1 = diff_cmd_baton->orig_path_1;
  const char *path2 = diff_cmd_baton->orig_path_2;
  /* If only property differences are shown, there's nothing to do. */
  if (diff_cmd_baton->properties_only)
    return SVN_NO_ERROR;
      SVN_ERR(svn_stream_printf_from_utf8(outstream,
               diff_cmd_baton->header_encoding, subpool,
               "Index: %s" APR_EOL_STR
               SVN_DIFF__EQUAL_STRING APR_EOL_STR,
               path));
      SVN_ERR(svn_stream_printf_from_utf8(outstream,
               diff_cmd_baton->header_encoding, subpool,
        SVN_ERR(svn_stream_printf_from_utf8(outstream,
                 diff_cmd_baton->header_encoding, subpool,
        SVN_ERR(svn_stream_printf_from_utf8(outstream,
                 diff_cmd_baton->header_encoding, subpool,
            SVN_ERR(svn_stream_printf_from_utf8(outstream,
                     diff_cmd_baton->header_encoding, subpool,
            SVN_ERR(svn_stream_printf_from_utf8(outstream,
                     diff_cmd_baton->header_encoding, subpool,
      apr_file_t *outfile;
      apr_file_t *errfile;
      const char *outfilename;
      const char *errfilename;
      svn_stream_t *stream;

      SVN_ERR(svn_stream_printf_from_utf8(outstream,
               diff_cmd_baton->header_encoding, subpool,
               "Index: %s" APR_EOL_STR
               SVN_DIFF__EQUAL_STRING APR_EOL_STR,
               path));
      /* We deal in streams, but svn_io_run_diff2() deals in file handles,
         unfortunately, so we need to make these temporary files, and then
         copy the contents to our stream. */
      SVN_ERR(svn_io_open_unique_file3(&outfile, &outfilename, NULL,
                                       svn_io_file_del_on_pool_cleanup,
                                       subpool, subpool));
      SVN_ERR(svn_io_open_unique_file3(&errfile, &errfilename, NULL,
                                       svn_io_file_del_on_pool_cleanup,
                                       subpool, subpool));

                               &exitcode, outfile, errfile,
      SVN_ERR(svn_io_file_close(outfile, subpool));
      SVN_ERR(svn_io_file_close(errfile, subpool));

      /* Now, open and copy our files to our output streams. */
      SVN_ERR(svn_stream_open_readonly(&stream, outfilename,
                                       subpool, subpool));
      SVN_ERR(svn_stream_copy3(stream, svn_stream_disown(outstream, subpool),
                               NULL, NULL, subpool));
      SVN_ERR(svn_stream_open_readonly(&stream, errfilename,
                                       subpool, subpool));
      SVN_ERR(svn_stream_copy3(stream, svn_stream_disown(errstream, subpool),
                               NULL, NULL, subpool));

          SVN_ERR(svn_stream_printf_from_utf8(outstream,
                   diff_cmd_baton->header_encoding, subpool,
                   "Index: %s" APR_EOL_STR
                   SVN_DIFF__EQUAL_STRING APR_EOL_STR,
                   path));
              SVN_ERR(print_git_diff_header(outstream, &label1, &label2,
                                            operation,
                                            copyfrom_rev,
            SVN_ERR(svn_diff_file_output_unified3(outstream, diff,
                     tmpfile1, tmpfile2, label1, label2,

  /* During repos->wc diff of a copy revision numbers obtained
   * from the working copy are always SVN_INVALID_REVNUM. */
  if (diff_cmd_baton->repos_wc_diff_target_is_copy)
    {
      if (rev1 == SVN_INVALID_REVNUM &&
          diff_cmd_baton->revnum1 != SVN_INVALID_REVNUM)
        rev1 = diff_cmd_baton->revnum1;

      if (rev2 == SVN_INVALID_REVNUM &&
          diff_cmd_baton->revnum2 != SVN_INVALID_REVNUM)
        rev2 = diff_cmd_baton->revnum2;
    }

                                 svn_diff_op_modified, NULL,
                                 SVN_INVALID_REVNUM, diff_cmd_baton));
                               original_props, diff_cmd_baton, scratch_pool));
  /* During repos->wc diff of a copy revision numbers obtained
   * from the working copy are always SVN_INVALID_REVNUM. */
  if (diff_cmd_baton->repos_wc_diff_target_is_copy)
    {
      if (rev1 == SVN_INVALID_REVNUM &&
          diff_cmd_baton->revnum1 != SVN_INVALID_REVNUM)
        rev1 = diff_cmd_baton->revnum1;

      if (rev2 == SVN_INVALID_REVNUM &&
          diff_cmd_baton->revnum2 != SVN_INVALID_REVNUM)
        rev2 = diff_cmd_baton->revnum2;
    }

                                 copyfrom_revision, diff_cmd_baton));
                                 svn_diff_op_added, NULL, SVN_INVALID_REVNUM,
                                 diff_cmd_baton));
                               original_props, diff_cmd_baton, scratch_pool));
diff_file_deleted(svn_wc_notify_state_t *state,
                  svn_boolean_t *tree_conflicted,
                  const char *path,
                  const char *tmpfile1,
                  const char *tmpfile2,
                  const char *mimetype1,
                  const char *mimetype2,
                  apr_hash_t *original_props,
                  void *diff_baton,
                  apr_pool_t *scratch_pool)
  if (diff_cmd_baton->no_diff_deleted)
    {
      SVN_ERR(svn_stream_printf_from_utf8(diff_cmd_baton->outstream,
                diff_cmd_baton->header_encoding, scratch_pool,
                "Index: %s (deleted)" APR_EOL_STR
                SVN_DIFF__EQUAL_STRING APR_EOL_STR,
                path));
    }
  else
    {
      if (tmpfile1)
        SVN_ERR(diff_content_changed(path,
                                     tmpfile1, tmpfile2,
                                     diff_cmd_baton->revnum1,
                                     diff_cmd_baton->revnum2,
                                     mimetype1, mimetype2,
                                     svn_diff_op_deleted, NULL,
                                     SVN_INVALID_REVNUM, diff_cmd_baton));
    }
static const svn_wc_diff_callbacks4_t diff_callbacks =
{
  diff_file_opened,
  diff_file_changed,
  diff_file_added,
  diff_file_deleted,
  diff_dir_deleted,
  diff_dir_opened,
  diff_dir_added,
  diff_dir_props_changed,
  diff_dir_closed
};
      1. path is not a URL and start_revision != end_revision
      2. path is not a URL and start_revision == end_revision
      3. path is a URL and start_revision != end_revision
      4. path is a URL and start_revision == end_revision
      5. path is not a URL and no revisions given
   other.  When path is a URL there is no working copy. Thus
/** Check if paths PATH_OR_URL1 and PATH_OR_URL2 are urls and if the
 * revisions REVISION1 and REVISION2 are local. If PEG_REVISION is not
 * unspecified, ensure that at least one of the two revisions is not
 * BASE or WORKING.
 * If PATH_OR_URL1 can only be found in the repository, set *IS_REPOS1
 * to TRUE. If PATH_OR_URL2 can only be found in the repository, set
 * *IS_REPOS2 to TRUE. */
            const char *path_or_url1,
            const char *path_or_url2,
  /* Revisions can be said to be local or remote.
   * BASE and WORKING are local revisions.  */
  if (peg_revision->kind != svn_opt_revision_unspecified &&
      is_local_rev1 && is_local_rev2)
    return svn_error_create(SVN_ERR_CLIENT_BAD_REVISION, NULL,
                            _("At least one revision must be something other "
                              "than BASE or WORKING when diffing a URL"));
  /* Working copy paths with non-local revisions get turned into
     URLs.  We don't do that here, though.  We simply record that it
     needs to be done, which is information that helps us choose our
     diff helper function.  */
  *is_repos1 = ! is_local_rev1 || svn_path_is_url(path_or_url1);
  *is_repos2 = ! is_local_rev2 || svn_path_is_url(path_or_url2);

/* Return in *RESOLVED_URL the URL which PATH_OR_URL@PEG_REVISION has in
 * REVISION. If the object has no location in REVISION, set *RESOLVED_URL
 * to NULL. */
static svn_error_t *
resolve_pegged_diff_target_url(const char **resolved_url,
                               svn_ra_session_t *ra_session,
                               const char *path_or_url,
                               const svn_opt_revision_t *peg_revision,
                               const svn_opt_revision_t *revision,
                               svn_client_ctx_t *ctx,
                               apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  /* Check if the PATH_OR_URL exists at REVISION. */
  err = svn_client__repos_locations(resolved_url, NULL,
                                    NULL, NULL,
                                    ra_session,
                                    path_or_url,
                                    peg_revision,
                                    revision,
                                    NULL,
                                    ctx, scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_CLIENT_UNRELATED_RESOURCES ||
          err->apr_err == SVN_ERR_FS_NOT_FOUND)
        {
          svn_error_clear(err);
          *resolved_url = NULL;
        }
      else
        return svn_error_trace(err);
    }

  return SVN_NO_ERROR;
}

/** Prepare a repos repos diff between PATH_OR_URL1 and
 * PATH_OR_URL2@PEG_REVISION, in the revision range REVISION1:REVISION2.
 * Indicate the corresponding node kinds in *KIND1 and *KIND2, and verify
 * that at least one of the diff targets exists.
                         svn_node_kind_t *kind1,
                         svn_node_kind_t *kind2,
                         const char *path_or_url1,
                         const char *path_or_url2,
  const char *abspath_or_url2;
  const char *abspath_or_url1;
  if (!svn_path_is_url(path_or_url2))
    SVN_ERR(svn_dirent_get_absolute(&abspath_or_url2, path_or_url2,
    abspath_or_url2 = path_or_url2;
  if (!svn_path_is_url(path_or_url1))
    SVN_ERR(svn_dirent_get_absolute(&abspath_or_url1, path_or_url1,
    abspath_or_url1 = path_or_url1;
  SVN_ERR(convert_to_url(url1, ctx->wc_ctx, abspath_or_url1,
  SVN_ERR(convert_to_url(url2, ctx->wc_ctx, abspath_or_url2,
     calculated for PATH_OR_URL2 override the one for PATH_OR_URL1
     (since the diff will be "applied" to URL2 anyway). */
  if (strcmp(*url1, path_or_url1) != 0)
    *base_path = path_or_url1;
  if (strcmp(*url2, path_or_url2) != 0)
    *base_path = path_or_url2;
      const char *resolved_url1;
      const char *resolved_url2;

      SVN_ERR(resolve_pegged_diff_target_url(&resolved_url2, *ra_session,
                                             path_or_url2, peg_revision,
                                             revision2, ctx, pool));

      SVN_ERR(svn_ra_reparent(*ra_session, *url1, pool));
      SVN_ERR(resolve_pegged_diff_target_url(&resolved_url1, *ra_session,
                                             path_or_url1, peg_revision,
                                             revision1, ctx, pool));

      /* Either or both URLs might have changed as a result of resolving
       * the PATH_OR_URL@PEG_REVISION's history. If only one of the URLs
       * could be resolved, use the same URL for URL1 and URL2, so we can
       * show a diff that adds or removes the object (see issue #4153). */
      if (resolved_url2)
          *url2 = resolved_url2;
          if (!resolved_url1)
            *url1 = resolved_url2;
      if (resolved_url1)
          *url1 = resolved_url1;
          if (!resolved_url2)
            *url2 = resolved_url1;

      /* Reparent the session, since *URL2 might have changed as a result
         the above call. */
      SVN_ERR(svn_ra_reparent(*ra_session, *url2, pool));
           (path_or_url2 == *url2) ? NULL : abspath_or_url2,
  SVN_ERR(svn_ra_check_path(*ra_session, "", *rev2, kind2, pool));
           (strcmp(path_or_url1, *url1) == 0) ? NULL : abspath_or_url1,
  SVN_ERR(svn_ra_check_path(*ra_session, "", *rev1, kind1, pool));
  if (*kind1 == svn_node_none && *kind2 == svn_node_none)
                                 _("Diff targets '%s' and '%s' were not found "
  else if (*kind1 == svn_node_none)
  else if (*kind2 == svn_node_none)
  /* If one of the targets is a file, use the parent directory as anchor. */
  if (*kind1 == svn_node_file || *kind2 == svn_node_file)
   This function is really svn_client_diff6().  If you read the public
   API description for svn_client_diff6(), it sounds quite Grand.  It
   Perhaps someday a brave soul will truly make svn_client_diff6()
                          _("Sorry, svn_client_diff6 was called in a way "
/* Try to get properties for LOCAL_ABSPATH and return them in the property
 * hash *PROPS. If there are no properties because LOCAL_ABSPATH is not
 * versioned, return an empty property hash. */
static svn_error_t *
get_props(apr_hash_t **props,
          const char *local_abspath,
          svn_wc_context_t *wc_ctx,
          apr_pool_t *result_pool,
          apr_pool_t *scratch_pool)
{
  svn_error_t *err;

  err = svn_wc_prop_list2(props, wc_ctx, local_abspath, result_pool,
                          scratch_pool);
  if (err)
    {
      if (err->apr_err == SVN_ERR_WC_PATH_NOT_FOUND ||
          err->apr_err == SVN_ERR_WC_NOT_WORKING_COPY ||
          err->apr_err == SVN_ERR_WC_UPGRADE_REQUIRED)
        {
          svn_error_clear(err);
          *props = apr_hash_make(result_pool);
        }
      else
        return svn_error_trace(err);
    }

  return SVN_NO_ERROR;
}

/* Produce a diff between two arbitrary files at LOCAL_ABSPATH1 and
 * LOCAL_ABSPATH2, using the diff callbacks from CALLBACKS.
 * Use PATH as the name passed to diff callbacks.
 * FILE1_IS_EMPTY and FILE2_IS_EMPTY are used as hints which diff callback
 * function to use to compare the files (added/deleted/changed).
 *
 * If ORIGINAL_PROPS_OVERRIDE is not NULL, use it as original properties
 * instead of reading properties from LOCAL_ABSPATH1. This is required when
 * a file replaces a directory, where LOCAL_ABSPATH1 is an empty file that
 * file content must be diffed against, but properties to diff against come
 * from the replaced directory. */
static svn_error_t *
do_arbitrary_files_diff(const char *local_abspath1,
                        const char *local_abspath2,
                        const char *path,
                        svn_boolean_t file1_is_empty,
                        svn_boolean_t file2_is_empty,
                        apr_hash_t *original_props_override,
                        const svn_wc_diff_callbacks4_t *callbacks,
                        struct diff_cmd_baton *diff_cmd_baton,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *scratch_pool)
{
  apr_hash_t *original_props;
  apr_hash_t *modified_props;
  apr_array_header_t *prop_changes;
  svn_string_t *original_mime_type = NULL;
  svn_string_t *modified_mime_type = NULL;

  if (ctx->cancel_func)
    SVN_ERR(ctx->cancel_func(ctx->cancel_baton));

  if (diff_cmd_baton->ignore_properties)
    {
      original_props = apr_hash_make(scratch_pool);
      modified_props = apr_hash_make(scratch_pool);
    }
  else
    {
      /* Try to get properties from either file. It's OK if the files do not
       * have properties, or if they are unversioned. */
      if (original_props_override)
        original_props = original_props_override;
      else
        SVN_ERR(get_props(&original_props, local_abspath1, ctx->wc_ctx,
                          scratch_pool, scratch_pool));
      SVN_ERR(get_props(&modified_props, local_abspath2, ctx->wc_ctx,
                        scratch_pool, scratch_pool));
    }

  SVN_ERR(svn_prop_diffs(&prop_changes, modified_props, original_props,
                         scratch_pool));

  if (!diff_cmd_baton->force_binary)
    {
      /* Try to determine the mime-type of each file. */
      original_mime_type = apr_hash_get(original_props, SVN_PROP_MIME_TYPE,
                                        APR_HASH_KEY_STRING);
      if (!file1_is_empty && !original_mime_type)
        {
          const char *mime_type;
          SVN_ERR(svn_io_detect_mimetype2(&mime_type, local_abspath1,
                                          ctx->mimetypes_map, scratch_pool));

          if (mime_type)
            original_mime_type = svn_string_create(mime_type, scratch_pool);
        }

      modified_mime_type = apr_hash_get(modified_props, SVN_PROP_MIME_TYPE,
                                        APR_HASH_KEY_STRING);
      if (!file2_is_empty && !modified_mime_type)
        {
          const char *mime_type;
          SVN_ERR(svn_io_detect_mimetype2(&mime_type, local_abspath1,
                                          ctx->mimetypes_map, scratch_pool));

          if (mime_type)
            modified_mime_type = svn_string_create(mime_type, scratch_pool);
        }
    }

  /* Produce the diff. */
  if (file1_is_empty && !file2_is_empty)
    SVN_ERR(callbacks->file_added(NULL, NULL, NULL, path,
                                  local_abspath1, local_abspath2,
                                  /* ### TODO get real revision info
                                   * for versioned files? */
                                  SVN_INVALID_REVNUM, SVN_INVALID_REVNUM,
                                  original_mime_type ?
                                    original_mime_type->data : NULL,
                                  modified_mime_type ?
                                    modified_mime_type->data : NULL,
                                  /* ### TODO get copyfrom? */
                                  NULL, SVN_INVALID_REVNUM,
                                  prop_changes, original_props,
                                  diff_cmd_baton, scratch_pool));
  else if (!file1_is_empty && file2_is_empty)
    SVN_ERR(callbacks->file_deleted(NULL, NULL, path,
                                    local_abspath1, local_abspath2,
                                    original_mime_type ?
                                      original_mime_type->data : NULL,
                                    modified_mime_type ?
                                      modified_mime_type->data : NULL,
                                    original_props,
                                    diff_cmd_baton, scratch_pool));
  else
    SVN_ERR(callbacks->file_changed(NULL, NULL, NULL, path,
                                    local_abspath1, local_abspath2,
                                    /* ### TODO get real revision info
                                     * for versioned files? */
                                    SVN_INVALID_REVNUM, SVN_INVALID_REVNUM,
                                    original_mime_type ?
                                      original_mime_type->data : NULL,
                                    modified_mime_type ?
                                      modified_mime_type->data : NULL,
                                    prop_changes, original_props,
                                    diff_cmd_baton, scratch_pool));

  return SVN_NO_ERROR;
}

struct arbitrary_diff_walker_baton {
  /* The root directories of the trees being compared. */
  const char *root1_abspath;
  const char *root2_abspath;

  /* TRUE if recursing within an added subtree of root2_abspath that
   * does not exist in root1_abspath. */
  svn_boolean_t recursing_within_added_subtree;

  /* TRUE if recursing within an administrative (.i.e. .svn) directory. */
  svn_boolean_t recursing_within_adm_dir;

  /* The absolute path of the adm dir if RECURSING_WITHIN_ADM_DIR is TRUE.
   * Else this is NULL.*/
  const char *adm_dir_abspath;

  /* A path to an empty file used for diffs that add/delete files. */
  const char *empty_file_abspath;

  const svn_wc_diff_callbacks4_t *callbacks;
  struct diff_cmd_baton *callback_baton;
  svn_client_ctx_t *ctx;
  apr_pool_t *pool;
} arbitrary_diff_walker_baton;

/* Forward declaration needed because this function has a cyclic
 * dependency with do_arbitrary_dirs_diff(). */
static svn_error_t *
arbitrary_diff_walker(void *baton, const char *local_abspath,
                      const apr_finfo_t *finfo,
                      apr_pool_t *scratch_pool);

/* Produce a diff between two arbitrary directories at LOCAL_ABSPATH1 and
 * LOCAL_ABSPATH2, using the provided diff callbacks to show file changes
 * and, for versioned nodes, property changes.
 *
 * If ROOT_ABSPATH1 and ROOT_ABSPATH2 are not NULL, show paths in diffs
 * relative to these roots, rather than relative to LOCAL_ABSPATH1 and
 * LOCAL_ABSPATH2. This is needed when crawling a subtree that exists
 * only within LOCAL_ABSPATH2. */
static svn_error_t *
do_arbitrary_dirs_diff(const char *local_abspath1,
                       const char *local_abspath2,
                       const char *root_abspath1,
                       const char *root_abspath2,
                       const svn_wc_diff_callbacks4_t *callbacks,
                       struct diff_cmd_baton *callback_baton,
                       svn_client_ctx_t *ctx,
                       apr_pool_t *scratch_pool)
{
  apr_file_t *empty_file;
  svn_node_kind_t kind1;

  struct arbitrary_diff_walker_baton b;

  /* If LOCAL_ABSPATH1 is not a directory, crawl LOCAL_ABSPATH2 instead
   * and compare it to LOCAL_ABSPATH1, showing only additions.
   * This case can only happen during recursion from arbitrary_diff_walker(),
   * because do_arbitrary_nodes_diff() prevents this from happening at
   * the root of the comparison. */
  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));
  b.recursing_within_added_subtree = (kind1 != svn_node_dir);

  b.root1_abspath = root_abspath1 ? root_abspath1 : local_abspath1;
  b.root2_abspath = root_abspath2 ? root_abspath2 : local_abspath2;
  b.recursing_within_adm_dir = FALSE;
  b.adm_dir_abspath = NULL;
  b.callbacks = callbacks;
  b.callback_baton = callback_baton;
  b.ctx = ctx;
  b.pool = scratch_pool;

  SVN_ERR(svn_io_open_unique_file3(&empty_file, &b.empty_file_abspath,
                                   NULL, svn_io_file_del_on_pool_cleanup,
                                   scratch_pool, scratch_pool));

  SVN_ERR(svn_io_dir_walk2(b.recursing_within_added_subtree ? local_abspath2
                                                            : local_abspath1,
                           0, arbitrary_diff_walker, &b, scratch_pool));

  return SVN_NO_ERROR;
}

/* An implementation of svn_io_walk_func_t.
 * Note: LOCAL_ABSPATH is the path being crawled and can be on either side
 * of the diff depending on baton->recursing_within_added_subtree. */
static svn_error_t *
arbitrary_diff_walker(void *baton, const char *local_abspath,
                      const apr_finfo_t *finfo,
                      apr_pool_t *scratch_pool)
{
  struct arbitrary_diff_walker_baton *b = baton;
  const char *local_abspath1;
  const char *local_abspath2;
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;
  const char *child_relpath;
  apr_hash_t *dirents1;
  apr_hash_t *dirents2;
  apr_hash_t *merged_dirents;
  apr_array_header_t *sorted_dirents;
  int i;
  apr_pool_t *iterpool;

  if (b->ctx->cancel_func)
    SVN_ERR(b->ctx->cancel_func(b->ctx->cancel_baton));

  if (finfo->filetype != APR_DIR)
    return SVN_NO_ERROR;

  if (b->recursing_within_adm_dir)
    {
      if (svn_dirent_skip_ancestor(b->adm_dir_abspath, local_abspath))
        return SVN_NO_ERROR;
      else
        {
          b->recursing_within_adm_dir = FALSE;
          b->adm_dir_abspath = NULL;
        }
    }
  else if (strcmp(svn_dirent_basename(local_abspath, scratch_pool),
                  SVN_WC_ADM_DIR_NAME) == 0)
    {
      b->recursing_within_adm_dir = TRUE;
      b->adm_dir_abspath = apr_pstrdup(b->pool, local_abspath);
      return SVN_NO_ERROR;
    }

  if (b->recursing_within_added_subtree)
    child_relpath = svn_dirent_skip_ancestor(b->root2_abspath, local_abspath);
  else
    child_relpath = svn_dirent_skip_ancestor(b->root1_abspath, local_abspath);
  if (!child_relpath)
    return SVN_NO_ERROR;

  local_abspath1 = svn_dirent_join(b->root1_abspath, child_relpath,
                                   scratch_pool);
  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));

  local_abspath2 = svn_dirent_join(b->root2_abspath, child_relpath,
                                   scratch_pool);
  SVN_ERR(svn_io_check_resolved_path(local_abspath2, &kind2, scratch_pool));

  if (kind1 == svn_node_dir)
    SVN_ERR(svn_io_get_dirents3(&dirents1, local_abspath1,
                                TRUE, /* only_check_type */
                                scratch_pool, scratch_pool));
  else
    dirents1 = apr_hash_make(scratch_pool);

  if (kind2 == svn_node_dir)
    {
      apr_hash_t *original_props;
      apr_hash_t *modified_props;
      apr_array_header_t *prop_changes;

      /* Show any property changes for this directory. */
      SVN_ERR(get_props(&original_props, local_abspath1, b->ctx->wc_ctx,
                        scratch_pool, scratch_pool));
      SVN_ERR(get_props(&modified_props, local_abspath2, b->ctx->wc_ctx,
                        scratch_pool, scratch_pool));
      SVN_ERR(svn_prop_diffs(&prop_changes, modified_props, original_props,
                             scratch_pool));
      if (prop_changes->nelts > 0)
        SVN_ERR(diff_props_changed(NULL, NULL, child_relpath,
                                   b->recursing_within_added_subtree,
                                   prop_changes, original_props,
                                   b->callback_baton, scratch_pool));

      /* Read directory entries. */
      SVN_ERR(svn_io_get_dirents3(&dirents2, local_abspath2,
                                  TRUE, /* only_check_type */
                                  scratch_pool, scratch_pool));
    }
  else
    dirents2 = apr_hash_make(scratch_pool);

  /* Compare dirents1 to dirents2 and show added/deleted/changed files. */
  merged_dirents = apr_hash_merge(scratch_pool, dirents1, dirents2,
                                  NULL, NULL);
  sorted_dirents = svn_sort__hash(merged_dirents,
                                  svn_sort_compare_items_as_paths,
                                  scratch_pool);
  iterpool = svn_pool_create(scratch_pool);
  for (i = 0; i < sorted_dirents->nelts; i++)
    {
      svn_sort__item_t elt = APR_ARRAY_IDX(sorted_dirents, i, svn_sort__item_t);
      const char *name = elt.key;
      svn_io_dirent2_t *dirent1;
      svn_io_dirent2_t *dirent2;
      const char *child1_abspath;
      const char *child2_abspath;

      svn_pool_clear(iterpool);

      if (b->ctx->cancel_func)
        SVN_ERR(b->ctx->cancel_func(b->ctx->cancel_baton));

      if (strcmp(name, SVN_WC_ADM_DIR_NAME) == 0)
        continue;

      dirent1 = apr_hash_get(dirents1, name, APR_HASH_KEY_STRING);
      if (!dirent1)
        {
          dirent1 = svn_io_dirent2_create(iterpool);
          dirent1->kind = svn_node_none;
        }
      dirent2 = apr_hash_get(dirents2, name, APR_HASH_KEY_STRING);
      if (!dirent2)
        {
          dirent2 = svn_io_dirent2_create(iterpool);
          dirent2->kind = svn_node_none;
        }

      child1_abspath = svn_dirent_join(local_abspath1, name, iterpool);
      child2_abspath = svn_dirent_join(local_abspath2, name, iterpool);

      if (dirent1->special)
        SVN_ERR(svn_io_check_resolved_path(child1_abspath, &dirent1->kind,
                                           iterpool));
      if (dirent2->special)
        SVN_ERR(svn_io_check_resolved_path(child1_abspath, &dirent2->kind,
                                           iterpool));

      if (dirent1->kind == svn_node_dir &&
          dirent2->kind == svn_node_dir)
        continue;

      /* Files that exist only in dirents1. */
      if (dirent1->kind == svn_node_file &&
          (dirent2->kind == svn_node_dir || dirent2->kind == svn_node_none))
        SVN_ERR(do_arbitrary_files_diff(child1_abspath, b->empty_file_abspath,
                                        svn_relpath_join(child_relpath, name,
                                                         iterpool),
                                        FALSE, TRUE, NULL,
                                        b->callbacks, b->callback_baton,
                                        b->ctx, iterpool));

      /* Files that exist only in dirents2. */
      if (dirent2->kind == svn_node_file &&
          (dirent1->kind == svn_node_dir || dirent1->kind == svn_node_none))
        {
          apr_hash_t *original_props;

          SVN_ERR(get_props(&original_props, child1_abspath, b->ctx->wc_ctx,
                            scratch_pool, scratch_pool));
          SVN_ERR(do_arbitrary_files_diff(b->empty_file_abspath, child2_abspath,
                                          svn_relpath_join(child_relpath, name,
                                                           iterpool),
                                          TRUE, FALSE, original_props,
                                          b->callbacks, b->callback_baton,
                                          b->ctx, iterpool));
        }

      /* Files that exist in dirents1 and dirents2. */
      if (dirent1->kind == svn_node_file && dirent2->kind == svn_node_file)
        SVN_ERR(do_arbitrary_files_diff(child1_abspath, child2_abspath,
                                        svn_relpath_join(child_relpath, name,
                                                         iterpool),
                                        FALSE, FALSE, NULL,
                                        b->callbacks, b->callback_baton,
                                        b->ctx, scratch_pool));

      /* Directories that only exist in dirents2. These aren't crawled
       * by this walker so we have to crawl them separately. */
      if (dirent2->kind == svn_node_dir &&
          (dirent1->kind == svn_node_file || dirent1->kind == svn_node_none))
        SVN_ERR(do_arbitrary_dirs_diff(child1_abspath, child2_abspath,
                                       b->root1_abspath, b->root2_abspath,
                                       b->callbacks, b->callback_baton,
                                       b->ctx, iterpool));
    }

  svn_pool_destroy(iterpool);

  return SVN_NO_ERROR;
}

/* Produce a diff between two files or two directories at LOCAL_ABSPATH1
 * and LOCAL_ABSPATH2, using the provided diff callbacks to show changes
 * in files. The files and directories involved may be part of a working
 * copy or they may be unversioned. For versioned files, show property
 * changes, too. */
static svn_error_t *
do_arbitrary_nodes_diff(const char *local_abspath1,
                        const char *local_abspath2,
                        const svn_wc_diff_callbacks4_t *callbacks,
                        struct diff_cmd_baton *callback_baton,
                        svn_client_ctx_t *ctx,
                        apr_pool_t *scratch_pool)
{
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;

  SVN_ERR(svn_io_check_resolved_path(local_abspath1, &kind1, scratch_pool));
  SVN_ERR(svn_io_check_resolved_path(local_abspath2, &kind2, scratch_pool));
  if (kind1 != kind2)
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("'%s' is not the same node kind as '%s'"),
                             local_abspath1, local_abspath2);

  if (kind1 == svn_node_file)
    SVN_ERR(do_arbitrary_files_diff(local_abspath1, local_abspath2,
                                    svn_dirent_basename(local_abspath2,
                                                        scratch_pool),
                                    FALSE, FALSE, NULL,
                                    callbacks, callback_baton,
                                    ctx, scratch_pool));
  else if (kind1 == svn_node_dir)
    SVN_ERR(do_arbitrary_dirs_diff(local_abspath1, local_abspath2,
                                   NULL, NULL,
                                   callbacks, callback_baton,
                                   ctx, scratch_pool));
  else
    return svn_error_createf(SVN_ERR_NODE_UNEXPECTED_KIND, NULL,
                             _("'%s' is not a file or directory"),
                             kind1 == svn_node_none ?
                              local_abspath1 : local_abspath2);
  return SVN_NO_ERROR;
}

   All other options are the same as those passed to svn_client_diff6(). */
    {
      const char *abspath2;

      SVN_ERR(svn_dirent_get_absolute(&abspath2, path2, pool));
      return svn_error_trace(do_arbitrary_nodes_diff(abspath1, abspath2,
                                                     callbacks,
                                                     callback_baton,
                                                     ctx, pool));
    }
/* Create an array of regular properties in PROP_HASH, filtering entry-props
 * and wc-props. Allocate the returned array in RESULT_POOL.
 * Use SCRATCH_POOL for temporary allocations. */
static apr_array_header_t *
make_regular_props_array(apr_hash_t *prop_hash,
                         apr_pool_t *result_pool,
                         apr_pool_t *scratch_pool)
{
  apr_array_header_t *regular_props;
  apr_hash_index_t *hi;

  regular_props = apr_array_make(result_pool, 0, sizeof(svn_prop_t));
  for (hi = apr_hash_first(scratch_pool, prop_hash); hi;
       hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      svn_string_t *value = svn__apr_hash_index_val(hi);
      svn_prop_kind_t prop_kind = svn_property_kind2(name);

      if (prop_kind == svn_prop_regular_kind)
        {
          svn_prop_t *prop = apr_palloc(scratch_pool, sizeof(svn_prop_t));

          prop->name = name;
          prop->value = value;
          APR_ARRAY_PUSH(regular_props, svn_prop_t) = *prop;
        }
    }

  return regular_props;
}

/* Create a hash of regular properties from PROP_HASH, filtering entry-props
 * and wc-props. Allocate the returned hash in RESULT_POOL.
 * Use SCRATCH_POOL for temporary allocations. */
static apr_hash_t *
make_regular_props_hash(apr_hash_t *prop_hash,
                        apr_pool_t *result_pool,
                        apr_pool_t *scratch_pool)
{
  apr_hash_t *regular_props;
  apr_hash_index_t *hi;

  regular_props = apr_hash_make(result_pool);
  for (hi = apr_hash_first(scratch_pool, prop_hash); hi;
       hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      svn_string_t *value = svn__apr_hash_index_val(hi);
      svn_prop_kind_t prop_kind = svn_property_kind2(name);

      if (prop_kind == svn_prop_regular_kind)
        apr_hash_set(regular_props, name, APR_HASH_KEY_STRING, value);
    }

  return regular_props;
}

/* Handle an added or deleted diff target file for a repos<->repos diff.
 *
 * Using the provided diff CALLBACKS and the CALLBACK_BATON, show the file
 * TARGET@PEG_REVISION as added or deleted, depending on SHOW_DELETION.
 * TARGET is a path relative to RA_SESSION's URL.
 * REV1 and REV2 are the revisions being compared.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
diff_repos_repos_added_or_deleted_file(const char *target,
                                       svn_revnum_t peg_revision,
                                       svn_revnum_t rev1,
                                       svn_revnum_t rev2,
                                       svn_boolean_t show_deletion,
                                      const char *empty_file,
                                       const svn_wc_diff_callbacks4_t
                                         *callbacks,
                                       struct diff_cmd_baton *callback_baton,
                                       svn_ra_session_t *ra_session,
                                       apr_pool_t *scratch_pool)
{
  const char *file_abspath;
  svn_stream_t *content;
  apr_hash_t *prop_hash;

  SVN_ERR(svn_stream_open_unique(&content, &file_abspath, NULL,
                                 svn_io_file_del_on_pool_cleanup,
                                 scratch_pool, scratch_pool));
  SVN_ERR(svn_ra_get_file(ra_session, target, peg_revision, content, NULL,
                          &prop_hash, scratch_pool));
  SVN_ERR(svn_stream_close(content));

  if (show_deletion)
    {
      SVN_ERR(callbacks->file_deleted(NULL, NULL,
                                      target, file_abspath, empty_file,
                                      apr_hash_get(prop_hash,
                                                   SVN_PROP_MIME_TYPE,
                                                   APR_HASH_KEY_STRING),
                                      NULL,
                                      make_regular_props_hash(
                                        prop_hash, scratch_pool, scratch_pool),
                                      callback_baton, scratch_pool));
    }
  else
    {
      SVN_ERR(callbacks->file_added(NULL, NULL, NULL,
                                    target, empty_file, file_abspath,
                                    rev1, rev2, NULL,
                                    apr_hash_get(prop_hash, SVN_PROP_MIME_TYPE,
                                                 APR_HASH_KEY_STRING),
                                    NULL, SVN_INVALID_REVNUM,
                                    make_regular_props_array(prop_hash,
                                                             scratch_pool,
                                                             scratch_pool),
                                    NULL, callback_baton, scratch_pool));
    }
    
  return SVN_NO_ERROR;
}

/* Handle an added or deleted diff target directory for a repos<->repos diff.
 *
 * Using the provided diff CALLBACKS and the CALLBACK_BATON, show the
 * directory TARGET@PEG_REVISION, and all of its children, as added or deleted,
 * depending on SHOW_DELETION. TARGET is a path relative to RA_SESSION's URL.
 * REV1 and REV2 are the revisions being compared.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
diff_repos_repos_added_or_deleted_dir(const char *target,
                                      svn_revnum_t revision,
                                      svn_revnum_t rev1,
                                      svn_revnum_t rev2,
                                      svn_boolean_t show_deletion,
                                      const char *empty_file,
                                      const svn_wc_diff_callbacks4_t
                                        *callbacks,
                                      struct diff_cmd_baton *callback_baton,
                                      svn_ra_session_t *ra_session,
                                      apr_pool_t *scratch_pool)
{
  apr_hash_t *dirents;
  apr_hash_t *props;
  apr_pool_t *iterpool;
  apr_hash_index_t *hi;

  SVN_ERR(svn_ra_get_dir2(ra_session, &dirents, NULL, &props,
                          target, revision, SVN_DIRENT_KIND,
                          scratch_pool));

  if (show_deletion)
    SVN_ERR(callbacks->dir_deleted(NULL, NULL, target, callback_baton,
                                   scratch_pool));
  else
    SVN_ERR(callbacks->dir_added(NULL, NULL, NULL, NULL,
                                 target, revision,
                                 NULL, SVN_INVALID_REVNUM,
                                 callback_baton, scratch_pool));
  if (props)
    {
      if (show_deletion)
        SVN_ERR(callbacks->dir_props_changed(NULL, NULL, target, FALSE,
                                             apr_array_make(scratch_pool, 0,
                                                            sizeof(svn_prop_t)),
                                             make_regular_props_hash(
                                               props, scratch_pool,
                                               scratch_pool),
                                             callback_baton, scratch_pool));
      else
        SVN_ERR(callbacks->dir_props_changed(NULL, NULL, target, TRUE,
                                             make_regular_props_array(
                                               props, scratch_pool,
                                               scratch_pool),
                                             NULL,
                                             callback_baton, scratch_pool));
    }

  iterpool = svn_pool_create(scratch_pool);
  for (hi = apr_hash_first(scratch_pool, dirents); hi; hi = apr_hash_next(hi))
    {
      const char *name = svn__apr_hash_index_key(hi);
      svn_dirent_t *dirent = svn__apr_hash_index_val(hi);
      const char *child_target;

      svn_pool_clear(iterpool);

      child_target = svn_relpath_join(target, name, iterpool);

      if (dirent->kind == svn_node_dir)
        SVN_ERR(diff_repos_repos_added_or_deleted_dir(child_target,
                                                      revision, rev1, rev2,
                                                      show_deletion,
                                                      empty_file,
                                                      callbacks,
                                                      callback_baton,
                                                      ra_session,
                                                      iterpool));
      else if (dirent->kind == svn_node_file)
        SVN_ERR(diff_repos_repos_added_or_deleted_file(child_target,
                                                       revision, rev1, rev2,
                                                       show_deletion,
                                                       empty_file,
                                                       callbacks,
                                                       callback_baton,
                                                       ra_session,
                                                       iterpool));
    }
  svn_pool_destroy(iterpool);

  if (!show_deletion)
    SVN_ERR(callbacks->dir_closed(NULL, NULL, NULL, target, TRUE,
                                  callback_baton, scratch_pool));

  return SVN_NO_ERROR;
}


/* Handle an added or deleted diff target for a repos<->repos diff.
 *
 * Using the provided diff CALLBACKS and the CALLBACK_BATON, show
 * TARGET@PEG_REVISION, and all of its children, if any, as added or deleted.
 * TARGET is a path relative to RA_SESSION's URL.
 * REV1 and REV2 are the revisions being compared.
 * Use SCRATCH_POOL for temporary allocations. */
static svn_error_t *
diff_repos_repos_added_or_deleted_target(const char *target1,
                                         const char *target2,
                                         svn_revnum_t rev1,
                                         svn_revnum_t rev2,
                                         svn_node_kind_t kind1,
                                         svn_node_kind_t kind2,
                                         const svn_wc_diff_callbacks4_t
                                           *callbacks,
                                         struct diff_cmd_baton *callback_baton,
                                         svn_ra_session_t *ra_session,
                                         apr_pool_t *scratch_pool)
{
  const char *existing_target;
  svn_revnum_t existing_rev;
  svn_node_kind_t existing_kind;
  svn_boolean_t show_deletion;
  const char *empty_file;

  SVN_ERR_ASSERT(kind1 == svn_node_none || kind2 == svn_node_none);

  /* Are we showing an addition or deletion? */
  show_deletion = (kind2 == svn_node_none);

  /* Which target is being added/deleted? Is it a file or a directory? */
  if (show_deletion)
    {
      existing_target = target1;
      existing_rev = rev1;
      existing_kind = kind1;
    }
  else
    {
      existing_target = target2;
      existing_rev = rev2;
      existing_kind = kind2;
    }

  /* All file content will be diffed against the empty file. */
  SVN_ERR(svn_io_open_unique_file3(NULL, &empty_file, NULL,
                                   svn_io_file_del_on_pool_cleanup,
                                   scratch_pool, scratch_pool));

  if (existing_kind == svn_node_file)
    {
      /* Get file content and show a diff against the empty file. */
      SVN_ERR(diff_repos_repos_added_or_deleted_file(existing_target,
                                                     existing_rev,
                                                     rev1, rev2,
                                                     show_deletion,
                                                     empty_file,
                                                     callbacks,
                                                     callback_baton,
                                                     ra_session,
                                                     scratch_pool));
    }
  else
    {
      /* Walk the added/deleted tree and show a diff for each child. */
      SVN_ERR(diff_repos_repos_added_or_deleted_dir(existing_target,
                                                    existing_rev,
                                                    rev1, rev2,
                                                    show_deletion,
                                                    empty_file,
                                                    callbacks,
                                                    callback_baton,
                                                    ra_session,
                                                    scratch_pool));
    }

  return SVN_NO_ERROR;
}
   PATH_OR_URL1 and PATH_OR_URL2 may be either URLs or the working copy paths.
   If PEG_REVISION is specified, PATH_OR_URL2 is the path at the peg revision,
   history from PATH_OR_URL2.
   All other options are the same as those passed to svn_client_diff6(). */
                 const char *path_or_url1,
                 const char *path_or_url2,
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;
                                   &kind1, &kind2, &ra_session,
                                   ctx, path_or_url1, path_or_url2,
  if (kind1 == svn_node_none || kind2 == svn_node_none)
    {
      /* One side of the diff does not exist.
       * Walk the tree that does exist, showing a series of additions
       * or deletions. */
      SVN_ERR(diff_repos_repos_added_or_deleted_target(target1, target2,
                                                       rev1, rev2,
                                                       kind1, kind2,
                                                       callbacks,
                                                       callback_baton,
                                                       ra_session,
                                                       pool));
      return SVN_NO_ERROR;
    }

                depth,
                extra_ra_session, rev1, TRUE /* walk_deleted_dirs */,
                TRUE /* text_deltas */,
                pool));
           depth, ignore_ancestry, TRUE /* text_deltas */,
/* Using CALLBACKS, show a REPOS->WC diff for a file TARGET, which in the
 * working copy is at FILE2_ABSPATH. KIND1 is the node kind of the repository
 * target (either svn_node_file or svn_node_none). REV is the revision the
 * working file is diffed against. RA_SESSION points at the URL of the file
 * in the repository and is used to get the file's repository-version content,
 * if necessary. The other parameters are as in diff_repos_wc(). */
static svn_error_t *
diff_repos_wc_file_target(const char *target,
                          const char *file2_abspath,
                          svn_node_kind_t kind1,
                          svn_revnum_t rev,
                          svn_boolean_t reverse,
                          svn_boolean_t show_copies_as_adds,
                          const svn_wc_diff_callbacks4_t *callbacks,
                          void *callback_baton,
                          svn_ra_session_t *ra_session,
                          svn_client_ctx_t *ctx,
                          apr_pool_t *scratch_pool)
{
  const char *file1_abspath;
  svn_stream_t *file1_content;
  svn_stream_t *file2_content;
  apr_hash_t *file1_props = NULL;
  apr_hash_t *file2_props;
  svn_boolean_t is_copy = FALSE;
  apr_hash_t *keywords = NULL;
  svn_string_t *keywords_prop;
  svn_subst_eol_style_t eol_style;
  const char *eol_str;

  /* Get content and props of file 1 (the remote file). */
  SVN_ERR(svn_stream_open_unique(&file1_content, &file1_abspath, NULL,
                                 svn_io_file_del_on_pool_cleanup,
                                 scratch_pool, scratch_pool));
  if (kind1 == svn_node_file)
    {
      if (show_copies_as_adds)
        SVN_ERR(svn_wc__node_get_origin(&is_copy, 
                                        NULL, NULL, NULL, NULL, NULL,
                                        ctx->wc_ctx, file2_abspath,
                                        FALSE, scratch_pool, scratch_pool));
      /* If showing copies as adds, diff against the empty file. */
      if (!(show_copies_as_adds && is_copy))
        SVN_ERR(svn_ra_get_file(ra_session, "", rev, file1_content,
                                NULL, &file1_props, scratch_pool));
    }

  SVN_ERR(svn_stream_close(file1_content));

  SVN_ERR(svn_wc_prop_list2(&file2_props, ctx->wc_ctx, file2_abspath,
                            scratch_pool, scratch_pool));

  /* We might have to create a normalised version of the working file. */
  svn_subst_eol_style_from_value(&eol_style, &eol_str,
                                 apr_hash_get(file2_props,
                                              SVN_PROP_EOL_STYLE,
                                              APR_HASH_KEY_STRING));
  keywords_prop = apr_hash_get(file2_props, SVN_PROP_KEYWORDS,
                               APR_HASH_KEY_STRING);
  if (keywords_prop)
    SVN_ERR(svn_subst_build_keywords2(&keywords, keywords_prop->data,
                                      NULL, NULL, 0, NULL,
                                      scratch_pool));
  if (svn_subst_translation_required(eol_style, SVN_SUBST_NATIVE_EOL_STR,
                                     keywords, FALSE, TRUE))
    {
      svn_stream_t *working_content;
      svn_stream_t *normalized_content;

      SVN_ERR(svn_stream_open_readonly(&working_content, file2_abspath,
                                       scratch_pool, scratch_pool));

      /* Create a temporary file and copy normalised data into it. */
      SVN_ERR(svn_stream_open_unique(&file2_content, &file2_abspath, NULL,
                                     svn_io_file_del_on_pool_cleanup,
                                     scratch_pool, scratch_pool));
      normalized_content = svn_subst_stream_translated(
                             file2_content, SVN_SUBST_NATIVE_EOL_STR,
                             TRUE, keywords, FALSE, scratch_pool);
      SVN_ERR(svn_stream_copy3(working_content, normalized_content,
                               ctx->cancel_func, ctx->cancel_baton,
                               scratch_pool));
    }

  if (kind1 == svn_node_file && !(show_copies_as_adds && is_copy))
    {
      SVN_ERR(callbacks->file_opened(NULL, NULL, target,
                                     reverse ? SVN_INVALID_REVNUM : rev,
                                     callback_baton, scratch_pool));

      if (reverse)
        SVN_ERR(callbacks->file_changed(NULL, NULL, NULL, target,
                                        file2_abspath, file1_abspath,
                                        SVN_INVALID_REVNUM, rev,
                                        apr_hash_get(file2_props,
                                                     SVN_PROP_MIME_TYPE,
                                                     APR_HASH_KEY_STRING),
                                        apr_hash_get(file1_props,
                                                     SVN_PROP_MIME_TYPE,
                                                     APR_HASH_KEY_STRING),
                                        make_regular_props_array(
                                          file1_props, scratch_pool,
                                          scratch_pool),
                                        file2_props,
                                        callback_baton, scratch_pool));
      else
        SVN_ERR(callbacks->file_changed(NULL, NULL, NULL, target,
                                        file1_abspath, file2_abspath,
                                        rev, SVN_INVALID_REVNUM,
                                        apr_hash_get(file1_props,
                                                     SVN_PROP_MIME_TYPE,
                                                     APR_HASH_KEY_STRING),
                                        apr_hash_get(file2_props,
                                                     SVN_PROP_MIME_TYPE,
                                                     APR_HASH_KEY_STRING),
                                        make_regular_props_array(
                                          file2_props, scratch_pool,
                                          scratch_pool),
                                        file1_props,
                                        callback_baton, scratch_pool));
    }
  else
    {
      if (reverse)
        {
          SVN_ERR(callbacks->file_deleted(NULL, NULL,
                                          target, file2_abspath, file1_abspath,
                                          apr_hash_get(file2_props,
                                                       SVN_PROP_MIME_TYPE,
                                                       APR_HASH_KEY_STRING),
                                          NULL,
                                          make_regular_props_hash(
                                            file2_props, scratch_pool,
                                            scratch_pool),
                                          callback_baton, scratch_pool));
        }
      else
        {
          SVN_ERR(callbacks->file_added(NULL, NULL, NULL, target,
                                        file1_abspath, file2_abspath,
                                        rev, SVN_INVALID_REVNUM,
                                        NULL,
                                        apr_hash_get(file2_props,
                                                     SVN_PROP_MIME_TYPE,
                                                     APR_HASH_KEY_STRING),
                                        NULL, SVN_INVALID_REVNUM,
                                        make_regular_props_array(
                                          file2_props, scratch_pool,
                                          scratch_pool),
                                        NULL,
                                        callback_baton, scratch_pool));
        }
    }

  return SVN_NO_ERROR;
}

   PATH_OR_URL1 may be either a URL or a working copy path.  PATH2 is a
   If PEG_REVISION is specified, then PATH_OR_URL1 is the path in the peg
   All other options are the same as those passed to svn_client_diff6(). */
diff_repos_wc(const char *path_or_url1,
  const char *abspath_or_url1;
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;
  svn_boolean_t is_copy;
  svn_revnum_t copyfrom_rev;
  const char *copy_source_repos_relpath;
  const char *copy_source_repos_root_url;
  if (!svn_path_is_url(path_or_url1))
    SVN_ERR(svn_dirent_get_absolute(&abspath_or_url1, path_or_url1, pool));
    abspath_or_url1 = path_or_url1;
  /* Convert path_or_url1 to a URL to feed to do_diff. */
  SVN_ERR(convert_to_url(&url1, ctx->wc_ctx, abspath_or_url1, pool, pool));
      SVN_ERR(svn_client__repos_locations(&url1, NULL, NULL, NULL,
                                          path_or_url1,
                                          revision1, NULL,

  /* Open an RA session to URL1 to figure out its node kind. */
  SVN_ERR(svn_client__open_ra_session_internal(&ra_session, NULL, url1,
                                               NULL, NULL, FALSE, TRUE,
                                               ctx, pool));
  /* Resolve the revision to use for URL1. */
  SVN_ERR(svn_client__get_revision_number(&rev, NULL, ctx->wc_ctx,
                                          (strcmp(path_or_url1, url1) == 0)
                                                    ? NULL : abspath_or_url1,
                                          ra_session, revision1, pool));
  SVN_ERR(svn_ra_check_path(ra_session, "", rev, &kind1, pool));

  /* Figure out the node kind of the local target. */
  SVN_ERR(svn_io_check_resolved_path(abspath2, &kind2, pool));

  callback_baton->ra_session = ra_session;
  if (!reverse)
    callback_baton->revnum1 = rev;
  else
    callback_baton->revnum2 = rev;

  /* Check if our diff target is a copied node. */
  SVN_ERR(svn_wc__node_get_origin(&is_copy, 
                                  &copyfrom_rev,
                                  &copy_source_repos_relpath,
                                  &copy_source_repos_root_url,
                                  NULL, NULL,
                                  ctx->wc_ctx, abspath2,
                                  FALSE, pool, pool));

  /* If both diff targets can be diffed as files, fetch the appropriate
   * file content from the repository and generate a diff against the
   * local version of the file.
   * However, if comparing the repository version of the file to the BASE
   * tree version we can use the diff editor to transmit a delta instead
   * of potentially huge file content. */
  if ((!rev2_is_base || is_copy) &&
      (kind1 == svn_node_file || kind1 == svn_node_none)
       && kind2 == svn_node_file)
    {
      SVN_ERR(diff_repos_wc_file_target(target, abspath2, kind1, rev,
                                        reverse, show_copies_as_adds,
                                        callbacks, callback_baton,
                                        ra_session, ctx, pool));

      return SVN_NO_ERROR;
    }

  /* Use the diff editor to generate the diff. */
  SVN_ERR(svn_wc__get_diff_editor(&diff_editor, &diff_edit_baton,
  SVN_ERR(svn_ra_reparent(ra_session, anchor_url, pool));
  if (is_copy)
    {
      const char *copyfrom_url;
      const char *copyfrom_parent_url;
      const char *copyfrom_basename;
      svn_depth_t copy_depth;

      callback_baton->repos_wc_diff_target_is_copy = TRUE;
      
      /* We're diffing a locally copied/moved directory.
       * Describe the copy source to the reporter instead of the copy itself.
       * Doing the latter would generate a single add_directory() call to the
       * diff editor which results in an unexpected diff (the copy would
       * be shown as deleted). */

      copyfrom_url = apr_pstrcat(pool, copy_source_repos_root_url, "/",
                                 copy_source_repos_relpath, (char *)NULL);
      svn_uri_split(&copyfrom_parent_url, &copyfrom_basename,
                    copyfrom_url, pool);
      SVN_ERR(svn_ra_reparent(ra_session, copyfrom_parent_url, pool));

      /* Tell the RA layer we want a delta to change our txn to URL1 */ 
      SVN_ERR(svn_ra_do_diff3(ra_session,
                              &reporter, &reporter_baton,
                              rev,
                              copyfrom_basename,
                              diff_depth,
                              ignore_ancestry,
                              TRUE,  /* text_deltas */
                              url1,
                              diff_editor, diff_edit_baton, pool));

      /* Report the copy source. */
      SVN_ERR(svn_wc__node_get_depth(&copy_depth, ctx->wc_ctx, abspath2,
                                     pool));
      SVN_ERR(reporter->set_path(reporter_baton, "", copyfrom_rev,
                                 copy_depth, FALSE, NULL, pool));
      
      /* Finish the report to generate the diff. */
      SVN_ERR(reporter->finish_report(reporter_baton, pool));
    }
  else
    {
      /* Tell the RA layer we want a delta to change our txn to URL1 */ 
      SVN_ERR(svn_ra_do_diff3(ra_session,
                              &reporter, &reporter_baton,
                              rev,
                              target,
                              diff_depth,
                              ignore_ancestry,
                              TRUE,  /* text_deltas */
                              url1,
                              diff_editor, diff_edit_baton, pool));

      /* Create a txn mirror of path2;  the diff editor will print
         diffs in reverse.  :-)  */
      SVN_ERR(svn_wc_crawl_revisions5(ctx->wc_ctx, abspath2,
                                      reporter, reporter_baton,
                                      FALSE, depth, TRUE,
                                      (! server_supports_depth),
                                      FALSE,
                                      ctx->cancel_func, ctx->cancel_baton,
                                      NULL, NULL, /* notification is N/A */
                                      pool));
    }
/* This is basically just the guts of svn_client_diff[_peg]6(). */
        const char *path_or_url1,
        const char *path_or_url2,
  SVN_ERR(check_paths(&is_repos1, &is_repos2, path_or_url1, path_or_url2,
          /* ### Ignores 'show_copies_as_adds'. */
                                   path_or_url1, path_or_url2,
                                   revision1, revision2,
      else /* path_or_url2 is a working copy path */
          SVN_ERR(diff_repos_wc(path_or_url1, revision1, peg_revision,
                                path_or_url2, revision2, FALSE, depth,
  else /* path_or_url1 is a working copy path */
          SVN_ERR(diff_repos_wc(path_or_url2, revision2, peg_revision,
                                path_or_url1, revision1, TRUE, depth,
      else /* path_or_url2 is a working copy path */
          SVN_ERR(diff_wc_wc(path_or_url1, revision1, path_or_url2, revision2,
/* Perform a summary diff between two working-copy paths.

   PATH1 and PATH2 are both working copy paths.  REVISION1 and
   REVISION2 are their respective revisions.

   All other options are the same as those passed to svn_client_diff6(). */
static svn_error_t *
diff_summarize_wc_wc(svn_client_diff_summarize_func_t summarize_func,
                     void *summarize_baton,
                     const char *path1,
                     const svn_opt_revision_t *revision1,
                     const char *path2,
                     const svn_opt_revision_t *revision2,
                     svn_depth_t depth,
                     svn_boolean_t ignore_ancestry,
                     const apr_array_header_t *changelists,
                     svn_client_ctx_t *ctx,
                     apr_pool_t *pool)
{
  svn_wc_diff_callbacks4_t *callbacks;
  void *callback_baton;
  const char *abspath1, *target1;
  svn_node_kind_t kind;

  SVN_ERR_ASSERT(! svn_path_is_url(path1));
  SVN_ERR_ASSERT(! svn_path_is_url(path2));

  /* Currently we support only the case where path1 and path2 are the
     same path. */
  if ((strcmp(path1, path2) != 0)
      || (! ((revision1->kind == svn_opt_revision_base)
             && (revision2->kind == svn_opt_revision_working))))
    return unsupported_diff_error
      (svn_error_create
       (SVN_ERR_INCORRECT_PARAMS, NULL,
        _("Summarized diffs are only supported between a path's text-base "
          "and its working files at this time")));

  /* Find the node kind of PATH1 so that we know whether the diff drive will
     be anchored at PATH1 or its parent dir. */
  SVN_ERR(svn_dirent_get_absolute(&abspath1, path1, pool));
  SVN_ERR(svn_wc_read_kind(&kind, ctx->wc_ctx, abspath1, FALSE, pool));
  target1 = (kind == svn_node_dir) ? "" : svn_dirent_basename(path1, pool);
  SVN_ERR(svn_client__get_diff_summarize_callbacks(
            &callbacks, &callback_baton, target1,
            summarize_func, summarize_baton, pool));

  SVN_ERR(svn_wc_diff6(ctx->wc_ctx,
                       abspath1,
                       callbacks, callback_baton,
                       depth,
                       ignore_ancestry, FALSE /* show_copies_as_adds */,
                       FALSE /* use_git_diff_format */, changelists,
                       ctx->cancel_func, ctx->cancel_baton,
                       pool));
  return SVN_NO_ERROR;
}

                           const char *path_or_url1,
                           const char *path_or_url2,
  svn_node_kind_t kind1;
  svn_node_kind_t kind2;
  svn_wc_diff_callbacks4_t *callbacks;
  void *callback_baton;
                                   &kind1, &kind2, &ra_session,
                                   ctx, path_or_url1, path_or_url2,
                                   revision1, revision2,
  if (kind1 == svn_node_none || kind2 == svn_node_none)
    {
      /* One side of the diff does not exist.
       * Walk the tree that does exist, showing a series of additions
       * or deletions. */
      SVN_ERR(svn_client__get_diff_summarize_callbacks(
                &callbacks, &callback_baton, target1,
                summarize_func, summarize_baton, pool));
      SVN_ERR(diff_repos_repos_added_or_deleted_target(target1, target2,
                                                       rev1, rev2,
                                                       kind1, kind2,
                                                       callbacks,
                                                       callback_baton,
                                                       ra_session,
                                                       pool));
      return SVN_NO_ERROR;
    }

  SVN_ERR(svn_client__get_diff_summarize_callbacks(
            &callbacks, &callback_baton,
            target1, summarize_func, summarize_baton, pool));

  SVN_ERR(svn_client__get_diff_editor(&diff_editor, &diff_edit_baton,
            depth,
            extra_ra_session, rev1, TRUE /* walk_deleted_dirs */,
            FALSE /* text_deltas */,
            callbacks, callback_baton,
            ctx->cancel_func, ctx->cancel_baton,
            NULL /* notify_func */, NULL /* notify_baton */, pool));
                  const char *path_or_url1,
                  const char *path_or_url2,
                  const apr_array_header_t *changelists,
  SVN_ERR(check_paths(&is_repos1, &is_repos2, path_or_url1, path_or_url2,
                                      path_or_url1, path_or_url2,
                                      revision1, revision2,
  else if (! is_repos1 && ! is_repos2)
    return diff_summarize_wc_wc(summarize_func, summarize_baton,
                                path_or_url1, revision1,
                                path_or_url2, revision2,
                                depth, ignore_ancestry,
                                changelists, ctx, pool);
   return unsupported_diff_error(
            svn_error_create(SVN_ERR_UNSUPPORTED_FEATURE, NULL,
                             _("Summarizing diff cannot compare repository "
                               "to WC")));
svn_client_diff6(const apr_array_header_t *options,
                 const char *path_or_url1,
                 const char *path_or_url2,
                 svn_boolean_t ignore_properties,
                 svn_boolean_t properties_only,
                 svn_stream_t *outstream,
                 svn_stream_t *errstream,
  svn_opt_revision_t peg_revision;

  if (ignore_properties && properties_only)
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                            _("Cannot ignore properties and show only "
                              "properties at the same time"));
  diff_cmd_baton.orig_path_1 = path_or_url1;
  diff_cmd_baton.orig_path_2 = path_or_url2;
  diff_cmd_baton.outstream = outstream;
  diff_cmd_baton.errstream = errstream;
  diff_cmd_baton.ignore_properties = ignore_properties;
  diff_cmd_baton.properties_only = properties_only;
  diff_cmd_baton.no_diff_deleted = no_diff_deleted;
                 path_or_url1, path_or_url2, revision1, revision2,
                 &peg_revision,
svn_client_diff_peg6(const apr_array_header_t *options,
                     const char *path_or_url,
                     svn_boolean_t ignore_properties,
                     svn_boolean_t properties_only,
                     svn_stream_t *outstream,
                     svn_stream_t *errstream,

  if (ignore_properties && properties_only)
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, NULL,
                            _("Cannot ignore properties and show only "
                              "properties at the same time"));
  diff_cmd_baton.orig_path_1 = path_or_url;
  diff_cmd_baton.orig_path_2 = path_or_url;
  diff_cmd_baton.outstream = outstream;
  diff_cmd_baton.errstream = errstream;
  diff_cmd_baton.ignore_properties = ignore_properties;
  diff_cmd_baton.properties_only = properties_only;
  diff_cmd_baton.no_diff_deleted = no_diff_deleted;
                 path_or_url, path_or_url, start_revision, end_revision,
                 peg_revision,
svn_client_diff_summarize2(const char *path_or_url1,
                           const char *path_or_url2,
                           path_or_url1, path_or_url2, revision1, revision2,
                           &peg_revision,
                           depth, ignore_ancestry, changelists, pool);
svn_client_diff_summarize_peg2(const char *path_or_url,
                           path_or_url, path_or_url,
                           start_revision, end_revision, peg_revision,
                           depth, ignore_ancestry, changelists, pool);