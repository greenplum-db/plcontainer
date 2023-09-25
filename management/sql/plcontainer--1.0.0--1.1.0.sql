CREATE OR REPLACE FUNCTION plcontainer_apply(
    "data" anytable, 
    func regproc,
    batch_size int DEFAULT 4096
) RETURNS SETOF record
AS '$libdir/plcontainer', 'apply'
LANGUAGE C VOLATILE STRICT;
