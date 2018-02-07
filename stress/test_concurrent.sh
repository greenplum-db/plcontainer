#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

# The number of connection x 5
NUM_CONN=$(($1))

# The number of iteration  
NUM_ITER=$(($2))

create_db (){
  echo "Create DB...."
  dropdb stress_test  
  createdb stress_test
  rm -rf log
  mkdir -p log
  rm results.log
  psql stress_test -f $GPHOME/share/postgresql/plcontainer/plcontainer_install.sql
  psql stress_test -f prepare_concurrent.sql
  echo "Create DB finished"
}

# the hostfile need to be existed, and contains all hosts include both sements and master
remove_db (){
  echo "Clean DB..."
  dropdb stress_test
  gpstop -arf
  gpssh -f /home/gpadmin/env/hostfile "docker ps -a | awk '{print \$1}' | xargs docker rm -f"
  gpssh -f /home/gpadmin/env/hostfile "rm -rf /tmp/plcontainer.*"
  echo "Clean DB finished"
}

# When the number of connections is set to a high number, be care of the size of swap memory.
# Otherwise, container will not be able to start and docker may hang.
test_concurrent () { 
  for ((i=1; i<=$NUM_CONN; i++)); do
    echo "Test $i has started"
    PSQL_LIST=""
    for ((j=1; j<=5; j++)); do 
      psql stress_test -f concurrent_${j}.sql >> log/test_${j}_${i}.log & pid=$!
      PSQL_LIST+=" $pid"
    done
  done
  trap "kill -9 $PSQL_LIST" SIGINT
  wait $PSQL_LIST
}

run_concurrent_job () {
  for ((m=1; m<=$NUM_ITER; m++)); do
    echo "---------------------------------"
    echo "The $m ITER has started"
    time test_concurrent
    echo "The $m ITER has done"
    echo "---------------------------------"
    echo ""
  done
  for ((j=1; j<=$NUM_CONN; j++)); do
    for ((k=1; k<=5; k++)); do
      cat log/test_${k}_${j}.log >> results.log
    done
  done
}

if [ $# != 2 ]; then
    echo "Usage: ./test_concurrent.sh num_of_connection num_of_iteration"
    exit 1
fi
if [ -z "$GPHOME" ]; then
    echo "Need to source greenplum_path.sh before running tests"
    exit 1
fi
echo "Tests must run on master node"
echo "Start to test"
time create_db
time run_concurrent_job
time remove_db

echo "Test finished, please check output results.log, if neither no error message nor container left, it could be considered the tests are passed."
