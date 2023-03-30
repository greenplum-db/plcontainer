DROP EXTENSION plcontainer CASCADE;

SELECT current_database(), pg_backend_pid() \gset
\c postgres
SELECT pg_terminate_backend(:pg_backend_pid);
-- Sleep until the container is killed by the child process of QD/QE
\! sleep 5
DROP DATABASE :current_database;
