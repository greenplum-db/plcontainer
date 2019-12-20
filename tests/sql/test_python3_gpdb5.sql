select pytestudt3( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] );
select pytestudt4( array[
                (1,array[1,2,3],array['a','b','c']),
                (2,array[2,3,4],array['b','c','d']),
                (3,array[3,4,5],array['c','d','e'])
            ]::test_type4[] );
select pytestudt5(null::test_type4[]);
select pytestudt5(array[null]::test_type4[]);
select pytestudt7();
select pytestudt9();
select * from unnest(pytestudt10());
select unnest(a) from (select pytestudt12() as a) as q;
select * from unnest(pytestudt14( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] ));
select * from pytestudt15( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] );
select pybadudtarr();
select pybadudtarr2();