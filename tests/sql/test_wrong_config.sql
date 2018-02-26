-- start_ignore
\! plcontainer  runtime-backup -f /tmp/test_backup_cfg_file_wrong_config
-- end_ignore

\! $(pwd)/test_wrong_config.sh 0
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 1
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 2
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 3
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 4
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 5
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 6
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 7
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 8
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 9
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 10
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 11
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 12
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 13
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 14
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 15
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 16
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 17
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 18
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 19
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 20
select plcontainer_refresh_local_config(true);

\! $(pwd)/test_wrong_config.sh 21
select plcontainer_refresh_local_config(true);

select plcontainer_show_local_config();

-- start_ignore
 \! plcontainer  runtime-restore -f /tmp/test_backup_cfg_file_wrong_config
-- end_ignore
