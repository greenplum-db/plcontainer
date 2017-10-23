-- Uninstalling PL/Container trusted language support

DROP VIEW plcontainer_refresh_config;
DROP VIEW plcontainer_show_config;

DROP FUNCTION plcontainer_refresh_local_config(verbose bool);
DROP FUNCTION plcontainer_show_local_config();

DROP LANGUAGE plcontainer CASCADE;
DROP FUNCTION plcontainer_call_handler();
