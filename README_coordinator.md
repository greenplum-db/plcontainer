## Overview
Coordinator is a background worker in PostgreSQL/Greenplum. It decouples container management from backend process, making plcontainer extension focus on connecting to a compute service and retrieve result to the upper nodes within the plan tree.

## How it works?

## Memory and Variables
### Shared Memory
* coordinator_address (protocol+address)
### Local Memory
* HTAB *client_info_map
	Map request_id(pid of QE) to its detailed info.
* HTAB *runtime_map
	Map the relation from runtime_id to docker configuration.
	See `plcontainer_configuration.xml` in data directory.
* 
## GUCs
* max_container_number
	The maximum number of containers allowed to exist concurrently.
* 
## Communication

## Cache
In the above communication model, for every function call, QE will send a request to coordinator and wait until a newly created container becomes ready. This could cause a huge time delay before the function really being executed. To minimize the time delay,  containers are cached for the same session
[to be continued]