-- Uninstalling PL/Container trusted language support

DROP VIEW IF EXISTS plcontainer_refresh_config;
DROP VIEW IF EXISTS plcontainer_show_config;

DROP FUNCTION IF EXISTS plcontainer_refresh_local_config(verbose bool);
DROP FUNCTION IF EXISTS plcontainer_show_local_config();
DROP FUNCTION IF EXISTS plcontainer_containers_summary();

DROP TYPE IF EXISTS container_summary_type;

DROP LANGUAGE IF EXISTS plcontainer CASCADE;
DROP FUNCTION IF EXISTS plcontainer_call_handler();
