#  -*- coding: utf-8 -*-
#  See http://subversion.apache.org for more information.
#    Licensed to the Apache Software Foundation (ASF) under one
#    or more contributor license agreements.  See the NOTICE file
#    distributed with this work for additional information
#    regarding copyright ownership.  The ASF licenses this file
#    to you under the Apache License, Version 2.0 (the
#    "License"); you may not use this file except in compliance
#    with the License.  You may obtain a copy of the License at
#      http://www.apache.org/licenses/LICENSE-2.0
#    Unless required by applicable law or agreed to in writing,
#    software distributed under the License is distributed on an
#    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#    KIND, either express or implied.  See the License for the
#    specific language governing permissions and limitations
#    under the License.
import sys, re, os, time, shutil, logging

logger = logging.getLogger()
from svntest import err
Skip = svntest.testcase.Skip_deco
SkipUnless = svntest.testcase.SkipUnless_deco
XFail = svntest.testcase.XFail_deco
Issues = svntest.testcase.Issues_deco
Issue = svntest.testcase.Issue_deco
Wimp = svntest.testcase.Wimp_deco
def make_diff_header(path, old_tag, new_tag, src_label=None, dst_label=None):
  versions described in parentheses by OLD_TAG and NEW_TAG. SRC_LABEL and
  DST_LABEL are paths or urls that are added to the diff labels if we're
  diffing against the repository or diffing two arbitrary paths.
  Return the header as an array of newline-terminated strings."""
  if src_label:
    src_label = src_label.replace('\\', '/')
    src_label = '\t(.../' + src_label + ')'
  else:
    src_label = ''
  if dst_label:
    dst_label = dst_label.replace('\\', '/')
    dst_label = '\t(.../' + dst_label + ')'
  else:
    dst_label = ''
    "--- " + path_as_shown + src_label + "\t(" + old_tag + ")\n",
    "+++ " + path_as_shown + dst_label + "\t(" + new_tag + ")\n",
    ]

def make_no_diff_deleted_header(path, old_tag, new_tag):
  """Generate the expected diff header for a deleted file PATH when in
  'no-diff-deleted' mode. (In that mode, no further details appear after the
  header.) Return the header as an array of newline-terminated strings."""
  path_as_shown = path.replace('\\', '/')
  return [
    "Index: " + path_as_shown + " (deleted)\n",
    "===================================================================\n",
def make_git_diff_header(target_path, repos_relpath,
                         old_tag, new_tag, add=False, src_label=None,
                         dst_label=None, delete=False, text_changes=True,
                         cp=False, mv=False, copyfrom_path=None):
  """ Generate the expected 'git diff' header for file TARGET_PATH.
  REPOS_RELPATH is the location of the path relative to the repository root.
  The old and new versions ("revision X", or "working copy") must be
  specified in OLD_TAG and NEW_TAG.
  SRC_LABEL and DST_LABEL are paths or urls that are added to the diff
  labels if we're diffing against the repository. ADD, DELETE, CP and MV
  denotes the operations performed on the file. COPYFROM_PATH is the source
  of a copy or move.  Return the header as an array of newline-terminated
  strings."""

  path_as_shown = target_path.replace('\\', '/')
  if src_label:
    src_label = src_label.replace('\\', '/')
    src_label = '\t(.../' + src_label + ')'
  else:
    src_label = ''
  if dst_label:
    dst_label = dst_label.replace('\\', '/')
    dst_label = '\t(.../' + dst_label + ')'
  else:
    dst_label = ''

  output = [
    "Index: " + path_as_shown + "\n",
    "===================================================================\n"
  ]
  if add:
    output.extend([
      "diff --git a/" + repos_relpath + " b/" + repos_relpath + "\n",
      "new file mode 10644\n",
    ])
    if text_changes:
      output.extend([
        "--- /dev/null\t(" + old_tag + ")\n",
        "+++ b/" + repos_relpath + dst_label + "\t(" + new_tag + ")\n"
      ])
  elif delete:
    output.extend([
      "diff --git a/" + repos_relpath + " b/" + repos_relpath + "\n",
      "deleted file mode 10644\n",
    ])
    if text_changes:
      output.extend([
        "--- a/" + repos_relpath + src_label + "\t(" + old_tag + ")\n",
        "+++ /dev/null\t(" + new_tag + ")\n"
      ])
  elif cp:
    output.extend([
      "diff --git a/" + copyfrom_path + " b/" + repos_relpath + "\n",
      "copy from " + copyfrom_path + "\n",
      "copy to " + repos_relpath + "\n",
    ])
    if text_changes:
      output.extend([
        "--- a/" + copyfrom_path + src_label + "\t(" + old_tag + ")\n",
        "+++ b/" + repos_relpath + "\t(" + new_tag + ")\n"
      ])
  elif mv:
    output.extend([
      "diff --git a/" + copyfrom_path + " b/" + path_as_shown + "\n",
      "rename from " + copyfrom_path + "\n",
      "rename to " + repos_relpath + "\n",
    ])
    if text_changes:
      output.extend([
        "--- a/" + copyfrom_path + src_label + "\t(" + old_tag + ")\n",
        "+++ b/" + repos_relpath + "\t(" + new_tag + ")\n"
      ])
  else:
    output.extend([
      "diff --git a/" + repos_relpath + " b/" + repos_relpath + "\n",
      "--- a/" + repos_relpath + src_label + "\t(" + old_tag + ")\n",
      "+++ b/" + repos_relpath + dst_label + "\t(" + new_tag + ")\n",
    ])
  return output

def make_diff_prop_header(path):
  """Return a property diff sub-header, as a list of newline-terminated
     strings."""
  return [
    "\n",
    "Property changes on: " + path.replace('\\', '/') + "\n",
    "___________________________________________________________________\n"
  ]

def make_diff_prop_val(plus_minus, pval):
  "Return diff for prop value PVAL, with leading PLUS_MINUS (+ or -)."
  if len(pval) > 0 and pval[-1] != '\n':
    return [plus_minus + pval + "\n","\\ No newline at end of property\n"]
  return [plus_minus + pval]

def make_diff_prop_deleted(pname, pval):
  """Return a property diff for deletion of property PNAME, old value PVAL.
     PVAL is a single string with no embedded newlines.  Return the result
     as a list of newline-terminated strings."""
  return [
    "Deleted: " + pname + "\n",
    "## -1 +0,0 ##\n"
  ] + make_diff_prop_val("-", pval)

def make_diff_prop_added(pname, pval):
  """Return a property diff for addition of property PNAME, new value PVAL.
     PVAL is a single string with no embedded newlines.  Return the result
     as a list of newline-terminated strings."""
  return [
    "Added: " + pname + "\n",
    "## -0,0 +1 ##\n",
  ] + make_diff_prop_val("+", pval)

def make_diff_prop_modified(pname, pval1, pval2):
  """Return a property diff for modification of property PNAME, old value
     PVAL1, new value PVAL2.  PVAL is a single string with no embedded
     newlines.  Return the result as a list of newline-terminated strings."""
  return [
    "Modified: " + pname + "\n",
    "## -1 +1 ##\n",
  ] + make_diff_prop_val("-", pval1) + make_diff_prop_val("+", pval2)

      logger.warn('Sought: %s' % excluded)
      logger.warn('Found:  %s' % line)
  repo_diff(wc_dir, 1, 4, check_add_a_file_in_a_subdir_reverse)
    None, 'diff', '-r', '1', sbox.ospath('A/D'))
    None, 'diff', '-r', '1', '-N', sbox.ospath('A/D'))
    None, 'diff', '-r', '1', sbox.ospath('A/D/G'))
  svntest.main.file_append(sbox.ospath('A/D/foo'), "a new file")
    1, 'diff', sbox.ospath('A/D/foo'))
  expected_output = \
    make_diff_header("iota", "revision 1", "revision 2") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_added("svn:eol-style", "native")
  expected_reverse_output = \
    make_diff_header("iota", "revision 2", "revision 1") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_deleted("svn:eol-style", "native")
  expected_rev1_output = \
    make_diff_header("iota", "revision 1", "working copy") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_added("svn:eol-style", "native")
  svntest.actions.run_and_verify_svn(None, expected_rev1_output, [],
  svntest.actions.run_and_verify_svn(None, expected_rev1_output, [],
@Issue(1019)
  theta_contents = open(os.path.join(sys.path[0], "theta.bin"), 'rb').read()
  theta_path = sbox.ospath('A/theta')
  mu_path = sbox.ospath('A/mu')
@Issue(977)
  iota_path = sbox.ospath('iota')
  newfile_path = sbox.ospath('A/D/newfile')
  mu_path = sbox.ospath('A/mu')
  # Two files in diff may be in any order.
  list1 = svntest.verify.UnorderedOutput(list1)

  svntest.verify.compare_and_display_lines('', '', list1, list2)
@Issue(891)
  A_path = sbox.ospath('A')
  mu_path = sbox.ospath('A/mu')
@Issue(2873)
  if check_update_a_file(diff_output) or check_add_a_file(diff_output):

  if not check_update_a_file(diff_output) or check_add_a_file(diff_output):
  if check_update_a_file(diff_output) or check_add_a_file(diff_output):
  A_alpha = sbox.ospath('A/B/E/alpha')
  A2_alpha = sbox.ospath('A2/B/E/alpha')
  A_alpha = sbox.ospath('A/B/E/alpha')
  A2_alpha = sbox.ospath('A2/B/E/alpha')
  A_path = sbox.ospath('A')
@Issue(1311)
  "diff between two file URLs"
  iota_path = sbox.ospath('iota')
  iota_copy_path = sbox.ospath('A/iota')
  iota_path = sbox.ospath('iota')
  verify_expected_output(out, "+pvalue")
  verify_expected_output(out, "+pvalue")  # fails at r7481
  verify_expected_output(out, "+pvalue")
  prefix_path = sbox.ospath('prefix_mydir')
  other_prefix_path = sbox.ospath('prefix_other')
    logger.warn("src is '%s' instead of '%s' and dest is '%s' instead of '%s'" %
  # Repos->WC diff of the file showing copies as adds
  exit_code, diff_output, err_output = svntest.main.run_svn(
                                         None, 'diff', '-r', '1',
                                         '--show-copies-as-adds', pi2_path)
  if check_diff_output(diff_output,
                       pi2_path,
                       'A') :
    raise svntest.Failure

  # Repos->WC of the containing directory
  # Repos->WC of the containing directory showing copies as adds
  exit_code, diff_output, err_output = svntest.main.run_svn(
    None, 'diff', '-r', '1', '--show-copies-as-adds', os.path.join('A', 'D'))

  if check_diff_output(diff_output,
                       pi_path,
                       'D') :
    raise svntest.Failure

  if check_diff_output(diff_output,
                       pi2_path,
                       'A') :
    raise svntest.Failure

  # WC->WC of the file showing copies as adds
  exit_code, diff_output, err_output = svntest.main.run_svn(
                                         None, 'diff',
                                         '--show-copies-as-adds', pi2_path)
  if check_diff_output(diff_output,
                       pi2_path,
                       'A') :
    raise svntest.Failure

  # Repos->WC diff of file after the rename
  # Repos->WC diff of file after the rename. The local file is not
  # a copy anymore (it has schedule "normal"), so --show-copies-as-adds
  # should have no effect.
  exit_code, diff_output, err_output = svntest.main.run_svn(
                                         None, 'diff', '-r', '1',
                                         '--show-copies-as-adds', pi2_path)
  if check_diff_output(diff_output,
                       pi2_path,
                       'M') :
    raise svntest.Failure

  # Repos->repos diff after the rename
  ### --show-copies-as-adds has no effect

  verify_expected_output(diff_output, "-v")
  iota_path = sbox.ospath('iota')
  iota_path = sbox.ospath('iota')
@Issue(2333)
  # Check a wc->wc diff
    None, 'diff', '--show-copies-as-adds', os.path.join('A', 'D'))
  # Check a repos->wc diff of the moved-here node before commit
  exit_code, diff_output, err_output = svntest.main.run_svn(
    None, 'diff', '-r', '1', '--show-copies-as-adds',
    os.path.join('A', 'D', 'I'))
  if check_diff_output(diff_output,
                       os.path.join('A', 'D', 'I', 'pi'),
                       'A') :
    raise svntest.Failure

  # Check a repos->wc diff of the moved-away node before commit
  exit_code, diff_output, err_output = svntest.main.run_svn(
    None, 'diff', '-r', '1', os.path.join('A', 'D', 'G'))
  if check_diff_output(diff_output,
                       os.path.join('A', 'D', 'G', 'pi'),
                       'D') :
    raise svntest.Failure

  # Commit
  # repos->repos with explicit URL arg
  exit_code, diff_output, err_output = svntest.main.run_svn(None, 'diff',
                                                            '-r', '1:2',
                                                            '^/A')
  if check_diff_output(diff_output,
                       os.path.join('D', 'G', 'pi'),
                       'D') :
    raise svntest.Failure
  if check_diff_output(diff_output,
                       os.path.join('D', 'I', 'pi'),
                       'A') :
    raise svntest.Failure

  # Go to the parent of the moved directory
  os.chdir(os.path.join('A','D'))
  # repos->wc diff in the parent
  if check_diff_output(diff_output,
                       os.path.join('G', 'pi'),
                       'D') :
    raise svntest.Failure
  if check_diff_output(diff_output,
                       os.path.join('I', 'pi'),
                       'A') :
  # repos->repos diff in the parent
  if check_diff_output(diff_output,
                       os.path.join('G', 'pi'),
                       'D') :
    raise svntest.Failure
  if check_diff_output(diff_output,
                       os.path.join('I', 'pi'),
                       'A') :
    raise svntest.Failure

  # Go to the move target directory
  os.chdir('I')

  # repos->wc diff while within the moved directory (should be empty)
  exit_code, diff_output, err_output = svntest.main.run_svn(None, 'diff',
                                                            '-r', '1')
  if diff_output:
    raise svntest.Failure

  # repos->repos diff while within the moved directory (should be empty)
  exit_code, diff_output, err_output = svntest.main.run_svn(None, 'diff',
                                                            '-r', '1:2')

  if diff_output:
  add_diff = \
    make_diff_prop_header("A") + \
    make_diff_prop_added("dirprop", "r2value") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_added("fileprop", "r2value")

  del_diff = \
    make_diff_prop_header("A") + \
    make_diff_prop_deleted("dirprop", "r2value") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_deleted("fileprop", "r2value")


  expected_output_r1_r2 = list(make_diff_header('A', 'revision 1', 'revision 2')
                               + add_diff[:6]
                               + make_diff_header('iota', 'revision 1',
                                                   'revision 2')
                               + add_diff[7:])

  expected_output_r2_r1 = list(make_diff_header('A', 'revision 2',
                                                'revision 1')
                               + del_diff[:6]
                               + make_diff_header('iota', 'revision 2',
                                                  'revision 1')
                               + del_diff[7:])

  expected_output_r1 = list(make_diff_header('A', 'revision 1',
                                             'working copy')
                            + add_diff[:6]
                            + make_diff_header('iota', 'revision 1',
                                               'working copy')
                            + add_diff[7:])
  expected_output_base_r1 = list(make_diff_header('A', 'working copy',
                                                  'revision 1')
                                 + del_diff[:6]
                                 + make_diff_header('iota', 'working copy',
                                                    'revision 1')
                                 + del_diff[7:])
  expected = svntest.verify.UnorderedOutput(expected_output_r1)
  expected = svntest.verify.UnorderedOutput(expected_output_base_r1)
  # presence of local mods (with the exception of diff header changes).
  expected = svntest.verify.UnorderedOutput(expected_output_r1)
  expected = svntest.verify.UnorderedOutput(expected_output_base_r1)
                                                "working copy") + [
  expected_output_r2_wc = \
    make_diff_header("A", "revision 2", "working copy") + \
    make_diff_prop_header("A") + \
    make_diff_prop_modified("dirprop", "r2value", "workingvalue") + \
    make_diff_prop_added("newdirprop", "newworkingvalue") + \
    make_diff_header("iota", "revision 2", "working copy") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_modified("fileprop", "r2value", "workingvalue") + \
    make_diff_prop_added("newfileprop", "newworkingvalue")
  diff_foo = [
    ] + make_diff_prop_header("foo") + \
    make_diff_prop_added("propname", "propvalue")
  diff_X = \
    make_diff_prop_header("X") + \
    make_diff_prop_added("propname", "propvalue")
  diff_X_bar = [
    ] + make_diff_prop_header("X/bar") + \
    make_diff_prop_added("propname", "propvalue")

  diff_X_r1_base = make_diff_header("X", "revision 1",
                                         "working copy") + diff_X
  diff_X_base_r3 = make_diff_header("X", "working copy",
                                         "revision 3") + diff_X
  diff_foo_r1_base = make_diff_header("foo", "revision 0",
                                             "revision 3") + diff_foo
  diff_foo_base_r3 = make_diff_header("foo", "revision 0",
                                             "revision 3") + diff_foo
  diff_X_bar_r1_base = make_diff_header("X/bar", "revision 0",
                                                 "revision 3") + diff_X_bar
  diff_X_bar_base_r3 = make_diff_header("X/bar", "revision 0",
                                                 "revision 3") + diff_X_bar

  expected_output_r1_base = svntest.verify.UnorderedOutput(diff_X_r1_base +
                                                           diff_X_bar_r1_base +
                                                           diff_foo_r1_base)
  expected_output_base_r3 = svntest.verify.UnorderedOutput(diff_foo_base_r3 +
                                                           diff_X_bar_base_r3 +
                                                           diff_X_base_r3)
  svntest.actions.run_and_verify_svn(None, expected_output_r1_base, [],
  svntest.actions.run_and_verify_svn(None, expected_output_r1_base, [],
  svntest.actions.run_and_verify_svn(None, expected_output_base_r3, [],
  # Check that a base->repos diff with copyfrom shows deleted and added lines.
  p = sbox.ospath
  # Diff summarize of a newly added file
  expected_diff = svntest.wc.State(wc_dir, {
    'iota': Item(status='A '),
    })
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('iota'), '-c1')
  # Reverse summarize diff of a newly added file
  expected_diff = svntest.wc.State(wc_dir, {
    'iota': Item(status='D '),
    })
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('iota'), '-c-1')
  # Diff summarize of a newly added directory
  expected_diff = svntest.wc.State(wc_dir, {
    'A/D':          Item(status='A '),
    'A/D/gamma':    Item(status='A '),
    'A/D/H':        Item(status='A '),
    'A/D/H/chi':    Item(status='A '),
    'A/D/H/psi':    Item(status='A '),
    'A/D/H/omega':  Item(status='A '),
    'A/D/G':        Item(status='A '),
    'A/D/G/pi':     Item(status='A '),
    'A/D/G/rho':    Item(status='A '),
    'A/D/G/tau':    Item(status='A '),
    })
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('A/D'), '-c1')
  # Reverse summarize diff of a newly added directory
  expected_diff = svntest.wc.State(wc_dir, {
    'A/D':          Item(status='D '),
    'A/D/gamma':    Item(status='D '),
    'A/D/H':        Item(status='D '),
    'A/D/H/chi':    Item(status='D '),
    'A/D/H/psi':    Item(status='D '),
    'A/D/H/omega':  Item(status='D '),
    'A/D/G':        Item(status='D '),
    'A/D/G/pi':     Item(status='D '),
    'A/D/G/rho':    Item(status='D '),
    'A/D/G/tau':    Item(status='D '),
    })
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('A/D'), '-c-1')
  # Add props to some items that will be deleted, and commit.
  sbox.simple_propset('prop', 'val',
                      'A/C',
                      'A/D/gamma',
                      'A/D/H/chi')
  sbox.simple_commit() # r2
  sbox.simple_update()
  # Content modification.
  svntest.main.file_append(p('A/mu'), 'new text\n')
  # Prop modification.
  sbox.simple_propset('prop', 'val', 'iota')

  # Both content and prop mods.
  svntest.main.file_append(p('A/D/G/tau'), 'new text\n')
  sbox.simple_propset('prop', 'val', 'A/D/G/tau')

  # File addition.
  svntest.main.file_append(p('newfile'), 'new text\n')
  svntest.main.file_append(p('newfile2'), 'new text\n')
  sbox.simple_add('newfile',
                  'newfile2')
  sbox.simple_propset('prop', 'val', 'newfile')

  # File deletion.
  sbox.simple_rm('A/B/lambda',
                 'A/D/gamma')

  # Directory addition.
  os.makedirs(p('P'))
  os.makedirs(p('Q/R'))
  svntest.main.file_append(p('Q/newfile'), 'new text\n')
  svntest.main.file_append(p('Q/R/newfile'), 'new text\n')
  sbox.simple_add('P',
                  'Q')
  sbox.simple_propset('prop', 'val',
                      'P',
                      'Q/newfile')

  # Directory deletion.
  sbox.simple_rm('A/D/H',
                 'A/C')

  # Commit, because diff-summarize handles repos-repos only.
  #svntest.main.run_svn(False, 'st', wc_dir)
  sbox.simple_commit() # r3
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('iota'), '-c3')
  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                p('iota'), '-c-3')
  # wc-wc diff summary for a directory.
    'A/mu':           Item(status='M '),
    'iota':           Item(status=' M'),
    'A/D/G/tau':      Item(status='MM'),
    'newfile':        Item(status='A '),
    'newfile2':       Item(status='A '),
    'P':              Item(status='A '),
    'Q':              Item(status='A '),
    'Q/newfile':      Item(status='A '),
    'Q/R':            Item(status='A '),
    'Q/R/newfile':    Item(status='A '),
    'A/B/lambda':     Item(status='D '),
    'A/C':            Item(status='D '),
    'A/D/gamma':      Item(status='D '),
    'A/D/H':          Item(status='D '),
    'A/D/H/chi':      Item(status='D '),
    'A/D/H/psi':      Item(status='D '),
    'A/D/H/omega':    Item(status='D '),
  expected_reverse_diff = svntest.wc.State(wc_dir, {
    'A/mu':           Item(status='M '),
    'iota':           Item(status=' M'),
    'A/D/G/tau':      Item(status='MM'),
    'newfile':        Item(status='D '),
    'newfile2':       Item(status='D '),
    'P':              Item(status='D '),
    'Q':              Item(status='D '),
    'Q/newfile':      Item(status='D '),
    'Q/R':            Item(status='D '),
    'Q/R/newfile':    Item(status='D '),
    'A/B/lambda':     Item(status='A '),
    'A/C':            Item(status='A '),
    'A/D/gamma':      Item(status='A '),
    'A/D/H':          Item(status='A '),
    'A/D/H/chi':      Item(status='A '),
    'A/D/H/psi':      Item(status='A '),
    'A/D/H/omega':    Item(status='A '),
    })

  svntest.actions.run_and_verify_diff_summarize(expected_diff,
                                                wc_dir, '-c3')
  svntest.actions.run_and_verify_diff_summarize(expected_reverse_diff,
                                                wc_dir, '-c-3')

#----------------------------------------------------------------------
  svntest.main.file_write(sbox.ospath('A/mu'),
@Issue(2121)
@Issue(2600)
  C_path = sbox.ospath('A/C')
  D_path = sbox.ospath('A/D')
  ### right now, we cannot denote that kappa is a local-add rather than a
  ### child of the A/D/C copy. thus, it appears in the status output as a
  ### (M)odified child.
  B_path = os.path.join('A', 'B')

  diff = make_diff_prop_header(".") + \
         make_diff_prop_added("foo1", "bar1") + \
         make_diff_prop_header("iota") + \
         make_diff_prop_added("foo2", "bar2") + \
         make_diff_prop_header("A") + \
         make_diff_prop_added("foo3", "bar3") + \
         make_diff_prop_header("A/B") + \
         make_diff_prop_added("foo4", "bar4")

  dot_header = make_diff_header(".", "revision 1", "working copy")
  iota_header = make_diff_header('iota', "revision 1", "working copy")
  A_header = make_diff_header('A', "revision 1", "working copy")
  B_header = make_diff_header(B_path, "revision 1", "working copy")

  expected_empty = svntest.verify.UnorderedOutput(dot_header + diff[:7])
  expected_files = svntest.verify.UnorderedOutput(dot_header + diff[:7]
                                                  + iota_header + diff[8:14])
  expected_immediates = svntest.verify.UnorderedOutput(dot_header + diff[:7]
                                                       + iota_header
                                                       + diff[8:14]
                                                       + A_header + diff[15:21])
  expected_infinity = svntest.verify.UnorderedOutput(dot_header + diff[:7]
                                                       + iota_header
                                                       + diff[8:14]
                                                       + A_header + diff[15:21]
                                                       + B_header + diff[22:])
  dot_header = make_diff_header(".", "revision 1", "revision 2")
  iota_header = make_diff_header('iota', "revision 1", "revision 2")
  A_header = make_diff_header('A', "revision 1", "revision 2")
  B_header = make_diff_header(B_path, "revision 1", "revision 2")

  expected_empty = svntest.verify.UnorderedOutput(dot_header + diff[:7])
  expected_files = svntest.verify.UnorderedOutput(dot_header + diff[:7]
                                                  + iota_header + diff[8:14])
  expected_immediates = svntest.verify.UnorderedOutput(dot_header + diff[:7]
                                                       + iota_header
                                                       + diff[8:14]
                                                       + A_header + diff[15:21])
  expected_infinity = svntest.verify.UnorderedOutput(dot_header + diff[:6]
                                                       + iota_header
                                                       + diff[8:14]
                                                       + A_header + diff[15:21]
                                                       + B_header + diff[22:])

  # Test repos-repos diff.
  diff_wc_repos = \
    make_diff_header("A/B", "revision 2", "working copy") + \
    make_diff_prop_header("A/B") + \
    make_diff_prop_modified("foo4", "bar4", "baz4") + \
    make_diff_header("A", "revision 2", "working copy") + \
    make_diff_prop_header("A") + \
    make_diff_prop_modified("foo3", "bar3", "baz3") + \
    make_diff_header("A/mu", "revision 1", "working copy") + [
    " This is the file 'mu'.\n",
    ] + make_diff_header("iota", "revision 2", "working copy") + [
    " This is the file 'iota'.\n",
    "+new text\n",
    ] + make_diff_prop_header("iota") + \
    make_diff_prop_modified("foo2", "bar2", "baz2") + \
    make_diff_header(".", "revision 2", "working copy") + \
    make_diff_prop_header(".") + \
    make_diff_prop_modified("foo1", "bar1", "baz1")

  expected_empty = svntest.verify.UnorderedOutput(diff_wc_repos[49:])
  expected_files = svntest.verify.UnorderedOutput(diff_wc_repos[33:])
  expected_immediates = svntest.verify.UnorderedOutput(diff_wc_repos[13:26]
                                                       +diff_wc_repos[33:])
@Issue(2920)
  time.sleep(1.1)
  svntest.main.file_append(sbox.ospath('A/mu'), "New mu content")
                       sbox.ospath('iota'))
  tau_path = sbox.ospath('A/D/G/tau')
  newfile_path = sbox.ospath('newfile')
  svntest.main.run_svn(None, "mkdir", sbox.ospath('newdir'))
  # 3) Test working copy summarize
  paths = ['A/mu', 'iota', 'A/D/G/tau', 'newfile', 'A/B/lambda',
           'newdir',]
  items = ['modified', 'none', 'modified', 'added', 'deleted', 'added',]
  kinds = ['file','file','file','file','file', 'dir',]
  props = ['none', 'modified', 'modified', 'none', 'none', 'none',]

  svntest.actions.run_and_verify_diff_summarize_xml(
    [], wc_dir, paths, items, props, kinds, wc_dir)

  paths_iota = ['iota',]
  items_iota = ['none',]
  kinds_iota = ['file',]
  props_iota = ['modified',]
    [], wc_dir, paths_iota, items_iota, props_iota, kinds_iota, '-c2',
    sbox.ospath('iota'))
  iota_path = sbox.ospath('iota')
  svntest.actions.run_and_verify_svn(None, [], err.INVALID_DIFF_OPTION,
    'for arg in sys.argv[1:]:\n  print(arg)\n')
    os.path.abspath(svntest.wc.text_base_path("iota")) + "\n",
    os.path.abspath("iota") + "\n"])
@XFail()
@Issue(3295)
# Diff against old revision of the parent directory of a removed and
# locally re-added file.
@XFail()
@Issue(3797)
  "diff -r1 of dir with removed-then-readded file"
def diff_git_format_wc_wc(sbox):
  "create a diff in git unidiff format for wc-wc"
  sbox.build()
  wc_dir = sbox.wc_dir
  iota_path = sbox.ospath('iota')
  mu_path = sbox.ospath('A/mu')
  new_path = sbox.ospath('new')
  lambda_path = sbox.ospath('A/B/lambda')
  lambda_copied_path = sbox.ospath('A/B/lambda_copied')
  alpha_path = sbox.ospath('A/B/E/alpha')
  alpha_copied_path = sbox.ospath('A/B/E/alpha_copied')

  svntest.main.file_append(iota_path, "Changed 'iota'.\n")
  svntest.main.file_append(new_path, "This is the file 'new'.\n")
  svntest.main.run_svn(None, 'add', new_path)
  svntest.main.run_svn(None, 'rm', mu_path)
  svntest.main.run_svn(None, 'cp', lambda_path, lambda_copied_path)
  svntest.main.run_svn(None, 'cp', alpha_path, alpha_copied_path)
  svntest.main.file_append(alpha_copied_path, "This is a copy of 'alpha'.\n")

  ### We're not testing moved paths

  expected_output = make_git_diff_header(lambda_copied_path,
                                         "A/B/lambda_copied",
                                         "revision 1", "working copy",
                                         copyfrom_path="A/B/lambda", cp=True,
                                         text_changes=False) \
  + make_git_diff_header(mu_path, "A/mu", "revision 1",
                                         "working copy",
                                         delete=True) + [
    "@@ -1 +0,0 @@\n",
    "-This is the file 'mu'.\n",
  ] + make_git_diff_header(alpha_copied_path, "A/B/E/alpha_copied",
                         "revision 0", "working copy",
                         copyfrom_path="A/B/E/alpha", cp=True,
                         text_changes=True) + [
    "@@ -1 +1,2 @@\n",
    " This is the file 'alpha'.\n",
    "+This is a copy of 'alpha'.\n",
  ] + make_git_diff_header(new_path, "new", "revision 0",
                           "working copy", add=True) + [
    "@@ -0,0 +1 @@\n",
    "+This is the file 'new'.\n",
  ] +  make_git_diff_header(iota_path, "iota", "revision 1",
                            "working copy") + [
    "@@ -1 +1,2 @@\n",
    " This is the file 'iota'.\n",
    "+Changed 'iota'.\n",
  ]

  expected = svntest.verify.UnorderedOutput(expected_output)

  svntest.actions.run_and_verify_svn(None, expected, [], 'diff',
                                     '--git', wc_dir)

def diff_git_format_url_wc(sbox):
  "create a diff in git unidiff format for url-wc"
  sbox.build()
  wc_dir = sbox.wc_dir
  repo_url = sbox.repo_url
  iota_path = sbox.ospath('iota')
  mu_path = sbox.ospath('A/mu')
  new_path = sbox.ospath('new')
  svntest.main.file_append(iota_path, "Changed 'iota'.\n")
  svntest.main.file_append(new_path, "This is the file 'new'.\n")
  svntest.main.run_svn(None, 'add', new_path)
  svntest.main.run_svn(None, 'rm', mu_path)

  ### We're not testing copied or moved paths

  svntest.main.run_svn(None, 'commit', '-m', 'Committing changes', wc_dir)
  svntest.main.run_svn(None, 'up', wc_dir)

  expected_output = make_git_diff_header(new_path, "new", "revision 0",
                                         "revision 2", add=True) + [
    "@@ -0,0 +1 @@\n",
    "+This is the file 'new'.\n",
  ] + make_git_diff_header(mu_path, "A/mu", "revision 1", "working copy",
                           delete=True) + [
    "@@ -1 +0,0 @@\n",
    "-This is the file 'mu'.\n",
  ] +  make_git_diff_header(iota_path, "iota", "revision 1",
                            "working copy") + [
    "@@ -1 +1,2 @@\n",
    " This is the file 'iota'.\n",
    "+Changed 'iota'.\n",
  ]

  expected = svntest.verify.UnorderedOutput(expected_output)

  svntest.actions.run_and_verify_svn(None, expected, [], 'diff',
                                     '--git',
                                     '--old', repo_url + '@1', '--new',
                                     wc_dir)

def diff_git_format_url_url(sbox):
  "create a diff in git unidiff format for url-url"
  sbox.build()
  wc_dir = sbox.wc_dir
  repo_url = sbox.repo_url
  iota_path = sbox.ospath('iota')
  mu_path = sbox.ospath('A/mu')
  new_path = sbox.ospath('new')
  svntest.main.file_append(iota_path, "Changed 'iota'.\n")
  svntest.main.file_append(new_path, "This is the file 'new'.\n")
  svntest.main.run_svn(None, 'add', new_path)
  svntest.main.run_svn(None, 'rm', mu_path)

  ### We're not testing copied or moved paths. When we do, we will not be
  ### able to identify them as copies/moves until we have editor-v2.

  svntest.main.run_svn(None, 'commit', '-m', 'Committing changes', wc_dir)
  svntest.main.run_svn(None, 'up', wc_dir)

  expected_output = make_git_diff_header("A/mu", "A/mu", "revision 1",
                                         "revision 2",
                                         delete=True) + [
    "@@ -1 +0,0 @@\n",
    "-This is the file 'mu'.\n",
    ] + make_git_diff_header("new", "new", "revision 0", "revision 2",
                             add=True) + [
    "@@ -0,0 +1 @@\n",
    "+This is the file 'new'.\n",
  ] +  make_git_diff_header("iota", "iota", "revision 1",
                            "revision 2") + [
    "@@ -1 +1,2 @@\n",
    " This is the file 'iota'.\n",
    "+Changed 'iota'.\n",
  ]

  expected = svntest.verify.UnorderedOutput(expected_output)

  svntest.actions.run_and_verify_svn(None, expected, [], 'diff',
                                     '--git',
                                     '--old', repo_url + '@1', '--new',
                                     repo_url + '@2')

# Regression test for an off-by-one error when printing intermediate context
# lines.
def diff_prop_missing_context(sbox):
  "diff for property has missing context"
  sbox.build()
  wc_dir = sbox.wc_dir

  iota_path = sbox.ospath('iota')
  prop_val = "".join([
       "line 1\n",
       "line 2\n",
       "line 3\n",
       "line 4\n",
       "line 5\n",
       "line 6\n",
       "line 7\n",
     ])
  svntest.main.run_svn(None,
                       "propset", "prop", prop_val, iota_path)

  expected_output = svntest.wc.State(wc_dir, {
      'iota'    : Item(verb='Sending'),
      })
  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.tweak('iota', wc_rev=2)
  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  prop_val = "".join([
               "line 3\n",
               "line 4\n",
               "line 5\n",
               "line 6\n",
             ])
  svntest.main.run_svn(None,
                       "propset", "prop", prop_val, iota_path)
  expected_output = make_diff_header(iota_path, 'revision 2',
                                     'working copy') + \
                    make_diff_prop_header(iota_path) + [
    "Modified: prop\n",
    "## -1,7 +1,4 ##\n",
    "-line 1\n",
    "-line 2\n",
    " line 3\n",
    " line 4\n",
    " line 5\n",
    " line 6\n",
    "-line 7\n",
  ]

  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', iota_path)

def diff_prop_multiple_hunks(sbox):
  "diff for property with multiple hunks"
  sbox.build()
  wc_dir = sbox.wc_dir

  iota_path = sbox.ospath('iota')
  prop_val = "".join([
       "line 1\n",
       "line 2\n",
       "line 3\n",
       "line 4\n",
       "line 5\n",
       "line 6\n",
       "line 7\n",
       "line 8\n",
       "line 9\n",
       "line 10\n",
       "line 11\n",
       "line 12\n",
       "line 13\n",
     ])
  svntest.main.run_svn(None,
                       "propset", "prop", prop_val, iota_path)

  expected_output = svntest.wc.State(wc_dir, {
      'iota'    : Item(verb='Sending'),
      })
  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.tweak('iota', wc_rev=2)
  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  prop_val = "".join([
               "line 1\n",
               "line 2\n",
               "line 3\n",
               "Add a line here\n",
               "line 4\n",
               "line 5\n",
               "line 6\n",
               "line 7\n",
               "line 8\n",
               "line 9\n",
               "line 10\n",
               "And add a line here\n",
               "line 11\n",
               "line 12\n",
               "line 13\n",
             ])
  svntest.main.run_svn(None,
                       "propset", "prop", prop_val, iota_path)
  expected_output = make_diff_header(iota_path, 'revision 2',
                                     'working copy') + \
                    make_diff_prop_header(iota_path) + [
    "Modified: prop\n",
    "## -1,6 +1,7 ##\n",
    " line 1\n",
    " line 2\n",
    " line 3\n",
    "+Add a line here\n",
    " line 4\n",
    " line 5\n",
    " line 6\n",
    "## -8,6 +9,7 ##\n",
    " line 8\n",
    " line 9\n",
    " line 10\n",
    "+And add a line here\n",
    " line 11\n",
    " line 12\n",
    " line 13\n",
  ]

  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', iota_path)
def diff_git_empty_files(sbox):
  "create a diff in git format for empty files"
  sbox.build()
  wc_dir = sbox.wc_dir
  iota_path = sbox.ospath('iota')
  new_path = sbox.ospath('new')
  svntest.main.file_write(iota_path, "")

  # Now commit the local mod, creating rev 2.
  expected_output = svntest.wc.State(wc_dir, {
    'iota' : Item(verb='Sending'),
    })

  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.add({
    'iota' : Item(status='  ', wc_rev=2),
    })

  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  svntest.main.file_write(new_path, "")
  svntest.main.run_svn(None, 'add', new_path)
  svntest.main.run_svn(None, 'rm', iota_path)

  expected_output = make_git_diff_header(new_path, "new", "revision 0",
                                         "working copy",
                                         add=True, text_changes=False) + [
  ] + make_git_diff_header(iota_path, "iota", "revision 2", "working copy",
                           delete=True, text_changes=False)

  # Two files in diff may be in any order.
  expected_output = svntest.verify.UnorderedOutput(expected_output)

  svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
                                     '--git', wc_dir)

def diff_git_with_props(sbox):
  "create a diff in git format showing prop changes"
  sbox.build()
  wc_dir = sbox.wc_dir
  iota_path = sbox.ospath('iota')
  new_path = sbox.ospath('new')
  svntest.main.file_write(iota_path, "")

  # Now commit the local mod, creating rev 2.
  expected_output = svntest.wc.State(wc_dir, {
    'iota' : Item(verb='Sending'),
    })

  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.add({
    'iota' : Item(status='  ', wc_rev=2),
    })

  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  svntest.main.file_write(new_path, "")
  svntest.main.run_svn(None, 'add', new_path)
  svntest.main.run_svn(None, 'propset', 'svn:eol-style', 'native', new_path)
  svntest.main.run_svn(None, 'propset', 'svn:keywords', 'Id', iota_path)

  expected_output = make_git_diff_header(new_path, "new",
                                         "revision 0", "working copy",
                                         add=True, text_changes=False) + \
                    make_diff_prop_header("new") + \
                    make_diff_prop_added("svn:eol-style", "native") + \
                    make_git_diff_header(iota_path, "iota",
                                         "revision 1", "working copy",
                                         text_changes=False) + \
                    make_diff_prop_header("iota") + \
                    make_diff_prop_added("svn:keywords", "Id")

  # Files in diff may be in any order.
  expected_output = svntest.verify.UnorderedOutput(expected_output)

  svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
                                     '--git', wc_dir)

@XFail()
@Issue(4010)
def diff_correct_wc_base_revnum(sbox):
  "diff WC-WC shows the correct base rev num"

  sbox.build()
  wc_dir = sbox.wc_dir
  iota_path = sbox.ospath('iota')
  svntest.main.file_write(iota_path, "")

  # Commit a local mod, creating rev 2.
  expected_output = svntest.wc.State(wc_dir, {
    'iota' : Item(verb='Sending'),
    })
  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.add({
    'iota' : Item(status='  ', wc_rev=2),
    })
  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  # Child's base is now 2; parent's is still 1.
  # Make a local mod.
  svntest.main.run_svn(None, 'propset', 'svn:keywords', 'Id', iota_path)

  expected_output = make_git_diff_header(iota_path, "iota",
                                         "revision 2", "working copy") + \
                    make_diff_prop_header("iota") + \
                    make_diff_prop_added("svn:keywords", "Id")

  # Diff the parent.
  svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
                                     '--git', wc_dir)

  # The same again, but specifying the target explicity. This should
  # give the same output.
  svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
                                     '--git', iota_path)

def diff_git_with_props_on_dir(sbox):
  "diff in git format showing prop changes on dir"
  sbox.build()
  wc_dir = sbox.wc_dir

  # Now commit the local mod, creating rev 2.
  expected_output = svntest.wc.State(wc_dir, {
    '.' : Item(verb='Sending'),
    })

  expected_status = svntest.actions.get_virginal_state(wc_dir, 1)
  expected_status.add({
    '' : Item(status='  ', wc_rev=2),
    })

  svntest.main.run_svn(None, 'ps', 'a','b', wc_dir)
  svntest.actions.run_and_verify_commit(wc_dir, expected_output,
                                        expected_status, None, wc_dir)

  was_cwd = os.getcwd()
  os.chdir(wc_dir)
  expected_output = make_git_diff_header(".", "", "revision 1",
                                         "revision 2",
                                         add=False, text_changes=False) + \
                    make_diff_prop_header("") + \
                    make_diff_prop_added("a", "b")

  svntest.actions.run_and_verify_svn(None, expected_output, [], 'diff',
                                     '-c2', '--git')
  os.chdir(was_cwd)

@Issue(3826)
def diff_abs_localpath_from_wc_folder(sbox):
  "diff absolute localpath from wc folder"
  sbox.build(read_only = True)
  wc_dir = sbox.wc_dir

  A_path = sbox.ospath('A')
  B_abs_path = os.path.abspath(sbox.ospath('A/B'))
  os.chdir(os.path.abspath(A_path))
  svntest.actions.run_and_verify_svn(None, None, [], 'diff', B_abs_path)

@Issue(3449)
def no_spurious_conflict(sbox):
  "no spurious conflict on update"
  sbox.build()
  wc_dir = sbox.wc_dir

  svntest.actions.do_sleep_for_timestamps()

  data_dir = os.path.join(os.path.dirname(sys.argv[0]), 'diff_tests_data')
  shutil.copyfile(os.path.join(data_dir, '3449_spurious_v1'),
                  sbox.ospath('3449_spurious'))
  svntest.actions.run_and_verify_svn(None, None, [],
                                     'add', sbox.ospath('3449_spurious'))
  sbox.simple_commit()
  shutil.copyfile(os.path.join(data_dir, '3449_spurious_v2'),
                  sbox.ospath('3449_spurious'))
  sbox.simple_commit()
  shutil.copyfile(os.path.join(data_dir, '3449_spurious_v3'),
                  sbox.ospath('3449_spurious'))
  sbox.simple_commit()

  svntest.actions.run_and_verify_svn(None, None, [],
                                     'update', '-r2', wc_dir)
  svntest.actions.run_and_verify_svn(None, None, [],
                                     'merge', '-c4', '^/', wc_dir)

  expected_status = svntest.actions.get_virginal_state(wc_dir, 2)
  expected_status.tweak('', status=' M')
  expected_status.add({
      '3449_spurious' : Item(status='M ', wc_rev=2),
      })
  svntest.actions.run_and_verify_status(wc_dir, expected_status)

  # This update produces a conflict in 1.6
  svntest.actions.run_and_verify_svn(None, None, [],
                                     'update', '--accept', 'postpone', wc_dir)
  expected_status.tweak(wc_rev=4)
  expected_status.tweak('3449_spurious', status='  ')
  svntest.actions.run_and_verify_status(wc_dir, expected_status)

def diff_two_working_copies(sbox):
  "diff between two working copies"
  sbox.build()
  wc_dir = sbox.wc_dir

  # Create a pristine working copy that will remain mostly unchanged
  wc_dir_old = sbox.add_wc_path('old')
  svntest.main.run_svn(None, 'co', sbox.repo_url, wc_dir_old)
  # Add a property to A/B/F in the pristine working copy
  svntest.main.run_svn(None, 'propset', 'newprop', 'propval-old\n',
                       os.path.join(wc_dir_old, 'A', 'B', 'F'))

  # Make changes to the first working copy:

  # removed nodes
  sbox.simple_rm('A/mu')
  sbox.simple_rm('A/D/H')

  # new nodes
  sbox.simple_mkdir('newdir')
  svntest.main.file_append(sbox.ospath('newdir/newfile'), 'new text\n')
  sbox.simple_add('newdir/newfile')
  sbox.simple_mkdir('newdir/newdir2') # should not show up in the diff

  # modified nodes
  sbox.simple_propset('newprop', 'propval', 'A/D')
  sbox.simple_propset('newprop', 'propval', 'A/D/gamma')
  svntest.main.file_append(sbox.ospath('A/B/lambda'), 'new text\n')

  # replaced nodes (files vs. directories) with property mods
  sbox.simple_rm('A/B/F')
  svntest.main.file_append(sbox.ospath('A/B/F'), 'new text\n')
  sbox.simple_add('A/B/F')
  sbox.simple_propset('newprop', 'propval-new\n', 'A/B/F')
  sbox.simple_rm('A/D/G/pi')
  sbox.simple_mkdir('A/D/G/pi')
  sbox.simple_propset('newprop', 'propval', 'A/D/G/pi')

  src_label = os.path.basename(wc_dir_old)
  dst_label = os.path.basename(wc_dir)
  expected_output = make_diff_header('newdir/newfile', 'working copy',
                                     'working copy',
                                     src_label, dst_label) + [
                      "@@ -0,0 +1 @@\n",
                      "+new text\n",
                    ] + make_diff_header('A/mu', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'mu'.\n",
                    ] + make_diff_header('A/B/F', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -0,0 +1 @@\n",
                      "+new text\n",
                    ] + make_diff_prop_header('A/B/F') + \
                        make_diff_prop_modified("newprop", "propval-old\n",
                                                "propval-new\n") + \
                    make_diff_header('A/B/lambda', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +1,2 @@\n",
                      " This is the file 'lambda'.\n",
                      "+new text\n",
                    ] + make_diff_header('A/D', 'working copy', 'working copy',
                                         src_label, dst_label) + \
                        make_diff_prop_header('A/D') + \
                        make_diff_prop_added("newprop", "propval") + \
                    make_diff_header('A/D/gamma', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + \
                        make_diff_prop_header('A/D/gamma') + \
                        make_diff_prop_added("newprop", "propval") + \
                    make_diff_header('A/D/G/pi', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'pi'.\n",
                    ] + make_diff_prop_header('A/D/G/pi') + \
                        make_diff_prop_added("newprop", "propval") + \
                    make_diff_header('A/D/H/chi', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'chi'.\n",
                    ] + make_diff_header('A/D/H/omega', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'omega'.\n",
                    ] + make_diff_header('A/D/H/psi', 'working copy',
                                         'working copy',
                                         src_label, dst_label) + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'psi'.\n",
                    ]
                    
  # Files in diff may be in any order.
  expected_output = svntest.verify.UnorderedOutput(expected_output)
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '--old', wc_dir_old,
                                     '--new', wc_dir)

def diff_deleted_url(sbox):
  "diff -cN of URL deleted in rN"
  sbox.build()
  wc_dir = sbox.wc_dir

  # remove A/D/H in r2
  sbox.simple_rm("A/D/H")
  sbox.simple_commit()

  # A diff of r2 with target A/D/H should show the removed children
  expected_output = make_diff_header("chi", "revision 1", "revision 2") + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'chi'.\n",
                    ] + make_diff_header("omega", "revision 1",
                                         "revision 2") + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'omega'.\n",
                    ] + make_diff_header("psi", "revision 1",
                                         "revision 2") + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'psi'.\n",
                    ]

  # Files in diff may be in any order.
  expected_output = svntest.verify.UnorderedOutput(expected_output)
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '-c2',
                                     sbox.repo_url + '/A/D/H')

def diff_arbitrary_files_and_dirs(sbox):
  "diff arbitrary files and dirs"
  sbox.build()
  wc_dir = sbox.wc_dir

  # diff iota with A/mu
  expected_output = make_diff_header("mu", "working copy", "working copy",
                                     "iota", "A/mu") + [
                      "@@ -1 +1 @@\n",
                      "-This is the file 'iota'.\n",
                      "+This is the file 'mu'.\n"
                    ]
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '--old', sbox.ospath('iota'),
                                     '--new', sbox.ospath('A/mu'))

  # diff A/B/E with A/D
  expected_output = make_diff_header("G/pi", "working copy", "working copy",
                                     "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'pi'.\n"
                    ] + make_diff_header("G/rho", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'rho'.\n"
                    ] + make_diff_header("G/tau", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'tau'.\n"
                    ] + make_diff_header("H/chi", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'chi'.\n"
                    ] + make_diff_header("H/omega", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'omega'.\n"
                    ] + make_diff_header("H/psi", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'psi'.\n"
                    ] + make_diff_header("alpha", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'alpha'.\n"
                    ] + make_diff_header("beta", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -1 +0,0 @@\n",
                      "-This is the file 'beta'.\n"
                    ] + make_diff_header("gamma", "working copy",
                                         "working copy", "B/E", "D") + [
                      "@@ -0,0 +1 @@\n",
                      "+This is the file 'gamma'.\n"
                    ]

  # Files in diff may be in any order.
  expected_output = svntest.verify.UnorderedOutput(expected_output)
  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '--old', sbox.ospath('A/B/E'),
                                     '--new', sbox.ospath('A/D'))

def diff_properties_only(sbox):
  "diff --properties-only"

  sbox.build()
  wc_dir = sbox.wc_dir

  expected_output = \
    make_diff_header("iota", "revision 1", "revision 2") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_added("svn:eol-style", "native")

  expected_reverse_output = \
    make_diff_header("iota", "revision 2", "revision 1") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_deleted("svn:eol-style", "native")

  expected_rev1_output = \
    make_diff_header("iota", "revision 1", "working copy") + \
    make_diff_prop_header("iota") + \
    make_diff_prop_added("svn:eol-style", "native")

  # Make a property change and a content change to 'iota'
  # Only the property change should be displayed by diff --properties-only
  sbox.simple_propset('svn:eol-style', 'native', 'iota')
  svntest.main.file_append(sbox.ospath('iota'), 'new text')

  sbox.simple_commit() # r2

  svntest.actions.run_and_verify_svn(None, expected_output, [],
                                     'diff', '--properties-only', '-r', '1:2',
                                     sbox.repo_url + '/iota')

  svntest.actions.run_and_verify_svn(None, expected_reverse_output, [],
                                     'diff', '--properties-only', '-r', '2:1',
                                     sbox.repo_url + '/iota')

  os.chdir(wc_dir)
  svntest.actions.run_and_verify_svn(None, expected_rev1_output, [],
                                     'diff', '--properties-only', '-r', '1',
                                     'iota')

  svntest.actions.run_and_verify_svn(None, expected_rev1_output, [],
                                     'diff', '--properties-only',
                                     '-r', 'PREV', 'iota')
              diff_renamed_dir,
              diff_url_against_local_mods,
              diff_preexisting_rev_against_local_add,
              diff_git_format_wc_wc,
              diff_git_format_url_wc,
              diff_git_format_url_url,
              diff_prop_missing_context,
              diff_prop_multiple_hunks,
              diff_git_empty_files,
              diff_git_with_props,
              diff_git_with_props_on_dir,
              diff_abs_localpath_from_wc_folder,
              no_spurious_conflict,
              diff_correct_wc_base_revnum,
              diff_two_working_copies,
              diff_deleted_url,
              diff_arbitrary_files_and_dirs,
              diff_properties_only,