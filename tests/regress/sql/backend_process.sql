\connect

SET plcontainer.backend_type = 'process';

CREATE OR REPLACE FUNCTION check_postgres() RETURNS bool AS $$
# container: plc_python_shared
import subprocess
try:
    output = subprocess.check_output(["ps", "-ef"], text=True, stderr=subprocess.STDOUT)
    return "postgres" in output
except subprocess.CalledProcessError as e:
    raise Exception(e.stdout)
$$ LANGUAGE plcontainer;

\! LOCAL_PROCESS_MODE=1 $GPHOME/bin/plcontainer_clients/py3client &> /tmp/plcontainer_py3client.log &

SELECT check_postgres();

SHOW plcontainer.backend_type;

\connect
