#!/bin/bash

if [ -z ${SERVER_RENEWAL_MODE} ]; then
    echo Erorr you must define SERVER_RENEWAL_MODE: "PRERENEWAL" or "RENEWAL"
    exit 1
fi

cd /home/hercuser/Hercules/sql-files/


# mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS hercuser_rodb;"
# mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS hercuser_rodblog;"

echo 1
mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS ragnarok;"


echo 2
mysql -u root -proot ragnarok < main.sql

echo 3
mysql -u root -proot ragnarok < logs.sql

if [[ ${SERVER_RENEWAL_MODE} == "PRERENEWAL" ]]; then
    echo 4
    mysql -u root -proot ragnarok < item_db.sql
    echo 5
    mysql -u root -proot ragnarok < mob_db.sql
    echo 6
    mysql -u root -proot ragnarok < mob_skill_db.sql
elif [[ ${SERVER_RENEWAL_MODE} == "RENEWAL" ]]; then
    mysql -u root -proot ragnarok < item_db_re.sql
    mysql -u root -proot ragnarok < mob_db_re.sql
    mysql -u root -proot ragnarok < mob_skill_db_re.sql
else
    echo "Unknow SERVER_RENEWAL_MODE MODE: $SERVER_RENEWAL_MODE"
    exit 1
fi

echo 7
mysql -u root -proot ragnarok < item_db2.sql
echo 8
mysql -u root -proot ragnarok < mob_db2.sql
echo 9
mysql -u root -proot ragnarok < mob_skill_db2.sql
echo 10
mysql -u root -proot -e "CREATE USER 'ragnarok'@'%' IDENTIFIED BY 'ragnarok'; GRANT ALL PRIVILEGES ON ragnarok.* TO 'ragnarok'@'%'; FLUSH PRIVILEGES;"

echo create file ${CHECK_FILE}
echo DATABASE SETUP FINISH wait 10 sec
sleep 10
echo end > ${CHECK_FILE}

echo ls /tmp
ls /tmp -la

