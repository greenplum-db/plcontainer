\! plcontainer  runtime-backup -f /tmp/conf_back

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

 \! plcontainer  runtime-restore -f /tmp/conf_back
