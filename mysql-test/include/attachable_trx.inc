--source include/have_debug.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

SET @debug_saved = @@global.debug;

--let $DEFAULT_SE = `SELECT @@default_storage_engine`

--echo
START TRANSACTION;

--echo
CREATE TABLE t1(a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(255));

--echo
INSERT INTO t1(b) VALUES ('aaa');
INSERT INTO t1(b) VALUES ('bbb');
INSERT INTO t1(b) VALUES ('ccc');

--echo
COMMIT;

if ($DEFAULT_SE == "InnoDB") {

###########################################################################

--echo 
--echo # 1. Start outer transaction with SERIALIZABLE isolation level.
--echo # Check that SELECT in inner transaction will not take locks.

--echo
--echo # Start transaction.
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;

--echo # Insert a new row into t1.
INSERT INTO t1(b) VALUES ('ddd');
SELECT * FROM t1;

--echo
--echo # [another connection]
--connect (con1,localhost,root,,)

--echo
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
START TRANSACTION;
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
SET @@global.debug = '-d,use_attachable_trx';
--disconnect con1

--echo
--echo # [default connection]
--connection default

--echo
ROLLBACK;

}  # if ($DEFAULT_SE == "InnoDB")

###########################################################################

--echo
--echo # 2. Check that inner transaction has different visibility scope than
--echo # the outer transaction.

--echo # Start READ ONLY transaction.
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION READ ONLY;

--echo # SELECT to actually start a transaction.
SELECT * FROM t1;

--echo
--echo # [another connection]
--connect (con1,localhost,root,,)

--echo
START TRANSACTION;
UPDATE t1 SET b = 'zzz' WHERE a = 2;
COMMIT;
--disconnect con1

--echo
--echo # [default connection]
--connection default

--echo
--echo # SELECT in the outer transaction doesn't see the changes.
SELECT * FROM t1;

--echo
--echo # SELECT in the inner transaction sees the changes.
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
SET @@global.debug = '-d,use_attachable_trx';

--echo
--echo # COMMIT the outer transaction.
COMMIT;

--echo # SELECT in the outer transaction now sees the changes.
SELECT * FROM t1;
COMMIT;

if ($DEFAULT_SE == "InnoDB") {

###########################################################################

--echo
--echo # 3. Check that the inner transaction does not reset a save point set in
--echo # the outer transaction.

--echo
--echo # Start transaction.
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION;
SELECT * FROM t1;

--echo
--echo # Set save point.
SAVEPOINT sp1;

--echo
--echo # Do some changes.
UPDATE t1 SET b = 'xxx' WHERE a = 2;
SELECT * FROM t1;

--echo
--echo # Do anything in the inner transaction.
SET @@global.debug = '+d,use_attachable_trx';
SELECT * FROM t1;
SET @@global.debug = '-d,use_attachable_trx';

--echo
--echo # Just make sure the changes are still there.
SELECT * FROM t1;

--echo
--echo # Rollback to the save point to make sure it was not reset.
ROLLBACK TO SAVEPOINT sp1;

--echo
--echo # Check that the changes have been reverted.
SELECT * FROM t1;

--echo
--echo # Commit.
COMMIT;

}  # if ($DEFAULT_SE == "InnoDB")

###########################################################################

--echo
--echo # Cleanup.
DROP TABLE t1;

--echo
SET @@global.debug = @debug_saved;

# Wait till we reached the initial number of concurrent sessions
--source include/wait_until_count_sessions.inc
