CREATE OR REPLACE FUNCTION hello() RETURNS text AS $$
# container: plc_python_shared
return "hello"
$$ LANGUAGE plcontainer;

1: SELECT hello();

2: SELECT "UP_TIME" FROM plcontainer_containers_info();

SELECT gp_inject_fault_infinite('before_get_container_info', 'reset', dbid)
  FROM gp_segment_configuration WHERE role='p' AND content= -1;

SELECT gp_inject_fault_infinite('before_get_container_info', 'suspend', dbid)
  FROM gp_segment_configuration WHERE role='p' AND content= -1;

2&: SELECT "UP_TIME", "MEMORY_USAGE(KB)", "CPU_USAGE", "UDF INFO" FROM plcontainer_containers_info();

1: SELECT pg_terminate_backend(pg_backend_pid());

SELECT gp_inject_fault_infinite('before_get_container_info', 'resume', dbid)
  FROM gp_segment_configuration WHERE role='p' AND content= -1;

2<:
