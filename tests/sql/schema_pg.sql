-- Creating UDTs
CREATE EXTENSION plcontainer;

CREATE TYPE type_record AS (
    first text,
    second int4
);

CREATE TYPE test_type AS (
    a bool,
    b smallint,
    c int,
    d bigint,
    e float4,
    f float8,
    g numeric,
    h varchar
);

CREATE TYPE test_type2 AS (
    a bool[],
    b smallint[],
    c int[],
    d bigint[],
    e float4[],
    f float8[],
    g numeric[],
    h varchar[]
);

CREATE TYPE test_type3 AS (
    a int8,
    b float8,
    c varchar
);

CREATE TYPE test_type4 AS (
    a int8,
    b float8[],
    c varchar[]
);

-- Creating Tables

CREATE TABLE users (
    fname text,
    lname text,
    username text,
    userid serial
);

CREATE TABLE taxonomy (
    id serial,
    name text
);

CREATE TABLE entry (
    accession text,
    eid serial,
    txid int2
);

CREATE TABLE sequences (
    eid int4,
    pid serial,
    product text,
    sequence text,
    multipart bool default 'false'
);

CREATE TABLE xsequences (
    pid int4,
    sequence text
);

CREATE TABLE unicode_test (
    testvalue  text
);

CREATE TABLE table_record (
    first text,
    second int4
);

-- Inserting some test data

INSERT INTO users (fname, lname, username) VALUES ('jane', 'doe', 'j_doe');
INSERT INTO users (fname, lname, username) VALUES ('john', 'doe', 'johnd');
INSERT INTO users (fname, lname, username) VALUES ('willem', 'doe', 'w_doe');
INSERT INTO users (fname, lname, username) VALUES ('rick', 'smith', 'slash');

INSERT INTO taxonomy (name) VALUES ('HIV I') ;
INSERT INTO taxonomy (name) VALUES ('HIV II') ;
INSERT INTO taxonomy (name) VALUES ('HCV') ;

INSERT INTO entry (accession, txid) VALUES ('A00001', '1') ;
INSERT INTO entry (accession, txid) VALUES ('A00002', '1') ;
INSERT INTO entry (accession, txid) VALUES ('A00003', '1') ;
INSERT INTO entry (accession, txid) VALUES ('A00004', '2') ;
INSERT INTO entry (accession, txid) VALUES ('A00005', '2') ;
INSERT INTO entry (accession, txid) VALUES ('A00006', '3') ;

INSERT INTO sequences (sequence, eid, product, multipart) VALUES ('ABCDEF', 1, 'env', 'true') ;
INSERT INTO xsequences (sequence, pid) VALUES ('GHIJKL', 1) ;
INSERT INTO sequences (sequence, eid, product) VALUES ('ABCDEF', 2, 'env') ;
INSERT INTO sequences (sequence, eid, product) VALUES ('ABCDEF', 3, 'env') ;
INSERT INTO sequences (sequence, eid, product) VALUES ('ABCDEF', 4, 'gag') ;
INSERT INTO sequences (sequence, eid, product) VALUES ('ABCDEF', 5, 'env') ;
INSERT INTO sequences (sequence, eid, product) VALUES ('ABCDEF', 6, 'ns1') ;
