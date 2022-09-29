set datestyle='ISO,MDY';
select rtestudt3( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] );

select rtestudt4( array[
                (1,array[1,2,3],array['a','b','c']),
                (2,array[2,3,4],array['b','c','d']),
                (3,array[3,4,5],array['c','d','e'])
            ]::test_type4[] );

select rtestudt5(null::test_type4[]);
select rtestudt5(array[null]::test_type4[]);
select rtestudt7();
select rtestudt9();
select rtestudt10();
select * from unnest(rtestudt14( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] ));
select * from rtestudt15( array[(1,1,'a'), (2,2,'b'), (3,3,'c')]::test_type3[] );