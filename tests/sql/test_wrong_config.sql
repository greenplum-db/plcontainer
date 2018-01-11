-- start_ignore
\! plcontainer  runtime-backup -f /tmp/test_backup_cfg_file_wrong_config
-- end_ignore

\! $(pwd)/test_wrong_config.sh 0
select pylog100();

\! $(pwd)/test_wrong_config.sh 1
select pylog100();

\! $(pwd)/test_wrong_config.sh 2
select pylog100();

\! $(pwd)/test_wrong_config.sh 3
select pylog100();

\! $(pwd)/test_wrong_config.sh 4
select pylog100();

\! $(pwd)/test_wrong_config.sh 5
select pylog100();

\! $(pwd)/test_wrong_config.sh 6
select pylog100();

\! $(pwd)/test_wrong_config.sh 7
select pylog100();

\! $(pwd)/test_wrong_config.sh 8
select pylog100();

\! $(pwd)/test_wrong_config.sh 9
select pylog100();

\! $(pwd)/test_wrong_config.sh 10
select pylog100();

\! $(pwd)/test_wrong_config.sh 11
select pylog100();

\! $(pwd)/test_wrong_config.sh 12
select pylog100();

\! $(pwd)/test_wrong_config.sh 13
select pylog100();

\! $(pwd)/test_wrong_config.sh 14
select pylog100();

\! $(pwd)/test_wrong_config.sh 15
select pylog100();

\! $(pwd)/test_wrong_config.sh 16
select pylog100();

\! $(pwd)/test_wrong_config.sh 17
select pylog100();

\! $(pwd)/test_wrong_config.sh 18
select pylog100();

-- start_ignore
 \! plcontainer  runtime-restore -f /tmp/test_backup_cfg_file_wrong_config
-- end_ignore
