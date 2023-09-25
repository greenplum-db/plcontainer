CREATE OR REPLACE FUNCTION plcontainer_apply(
    "data" anytable, 
    func regproc,
    config jsonb DEFAULT 4096
) RETURNS SETOF record
AS '$libdir/plcontainer', 'apply'
LANGUAGE C VOLATILE STRICT;
