/*
 * sqlite-test.c -- test the stream functions
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

#include "private/svn_sqlite.h"
#include "../svn_test.h"

static svn_error_t *
open_db(svn_sqlite__db_t **sdb,
        const char *db_name,
        const char *const *statements,
        apr_pool_t *pool)
{
  const char *db_dir, *db_abspath;

  SVN_ERR(svn_dirent_get_absolute(&db_dir, "sqlite-test-tmp", pool));
  SVN_ERR(svn_io_remove_dir2(db_dir, TRUE, NULL, NULL, pool));
  SVN_ERR(svn_io_make_dir_recursively(db_dir, pool));
  svn_test_add_dir_cleanup(db_dir);

  db_abspath = svn_dirent_join(db_dir, db_name, pool);

  SVN_ERR(svn_sqlite__open(sdb, db_abspath, svn_sqlite__mode_rwcreate,
                           statements, 0, NULL, pool, pool));

  return SVN_NO_ERROR;
}

static svn_error_t *
error_second(svn_sqlite__context_t *sctx,
             int argc,
             svn_sqlite__value_t *values[],
             apr_pool_t *scratch_pool)
{
  static int i = 0;

  if (++i == 2)
    svn_sqlite__result_error(sctx, "fake error", 0);
  else
    svn_sqlite__result_int64(sctx, 1);
  
  return SVN_NO_ERROR;
}

static svn_error_t *
test_sqlite_reset(apr_pool_t *pool)
{
  svn_sqlite__db_t *sdb;
  svn_sqlite__stmt_t *stmt;
  svn_boolean_t have_row;
  const char *value;

  static const char *const statements[] = {
    "CREATE TABLE reset ("
    "    one TEXT NOT NULL PRIMARY KEY,"
    "    two TEXT"
    ");"
    "INSERT INTO reset(one, two) VALUES ('foo', 'bar');"
    "INSERT INTO reset(one, two) VALUES ('zig', 'zag')",

    "SELECT one FROM reset WHERE two IS NOT NULL AND error_second(one) "
    "ORDER BY one",

    NULL
  };

  SVN_ERR(open_db(&sdb, "reset", statements, pool));
  SVN_ERR(svn_sqlite__create_scalar_function(sdb, "error_second",
                                             1, FALSE /* deterministic */,
                                             error_second, NULL));
  SVN_ERR(svn_sqlite__exec_statements(sdb, 0));
  SVN_ERR(svn_sqlite__get_statement(&stmt, sdb, 1));

  /* First step is OK. */
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  SVN_TEST_ASSERT(have_row);
  value = svn_sqlite__column_text(stmt, 0, NULL);
  SVN_TEST_ASSERT(value && !strcmp(value, "foo"));

  /* Second step fails. */
  SVN_TEST_ASSERT_ERROR(svn_sqlite__step(&have_row, stmt),
                        SVN_ERR_SQLITE_ERROR);

  /* The svn_sqlite__step wrapper calls svn_sqlite__reset when step
     fails so the reset call here is a no-op.  The first step can be
     repeated. */ 
  SVN_ERR(svn_sqlite__reset(stmt));
  SVN_ERR(svn_sqlite__step(&have_row, stmt));
  SVN_TEST_ASSERT(have_row);
  value = svn_sqlite__column_text(stmt, 0, NULL);
  SVN_TEST_ASSERT(value && !strcmp(value, "foo"));
  SVN_ERR(svn_sqlite__reset(stmt));

  return SVN_NO_ERROR;
}


static int max_threads = 1;

static struct svn_test_descriptor_t test_funcs[] =
  {
    SVN_TEST_NULL,
    SVN_TEST_PASS2(test_sqlite_reset,
                   "sqlite reset"),
    SVN_TEST_NULL
  };

SVN_TEST_MAIN
