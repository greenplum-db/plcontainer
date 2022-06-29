#!/bin/bash -l

set -exo pipefail

function _main() {
    # Start gpdb cluster
    export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1
    source /usr/local/greenplum-db-devel/greenplum_path.sh
    source gpdb_src/gpAux/gpdemo/gpdemo-env.sh
    # Run testing
    pushd plcontainer_artifacts
    time cmake --build . --target install
    time plcontainer image-add -f plcontainer_python3_shared.tar.gz
    # TODO for now drop logging for test maybe bring it back in the future
    time plcontainer runtime-add -r plc_python_shared -i python39.alpine -l python3
    time plcontainer runtime-add -r plc_python_shared_oom -i python39.alpine -l python3 -s memory_mb=100
    time cmake --build . --target testpy39

    popd
}

_main
