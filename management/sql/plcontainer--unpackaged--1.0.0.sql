-- Uninstalling PL/Container trusted language support

DROP VIEW IF EXISTS plcontainer_refresh_config;
DROP VIEW IF EXISTS plcontainer_show_config;

DROP FUNCTION IF EXISTS plcontainer_refresh_local_config(verbose bool);
DROP FUNCTION IF EXISTS plcontainer_show_local_config();

DROP LANGUAGE IF EXISTS plcontainer CASCADE;
DROP FUNCTION IF EXISTS plcontainer_call_handler();
