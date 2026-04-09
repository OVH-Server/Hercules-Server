#!/bin/bash

if [ -z ${SERVER_RENEWAL_MODE} ]; then
    echo Erorr you must define SERVER_RENEWAL_MODE: "PRERENEWAL" or "RENEWAL"
    exit 1
fi

cd /home/hercuser/Hercules/sql-files/

# mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS hercuser_rodb;"
# mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS hercuser_rodblog;"

mysql -u root -proot -e "CREATE DATABASE IF NOT EXISTS ragnarok;"


mysql -u root -proot ragnarok < main.sql
mysql -u root -proot ragnarok < logs.sql

if [[ ${SERVER_RENEWAL_MODE} == "PRERENEWAL" ]]; then
    mysql -u root -proot ragnarok < item_db.sql
    mysql -u root -proot ragnarok < mob_db.sql
    mysql -u root -proot ragnarok < mob_skill_db.sql
elif [[ ${SERVER_RENEWAL_MODE} == "RENEWAL" ]]; then
    mysql -u root -proot ragnarok < item_db_re.sql
    mysql -u root -proot ragnarok < mob_db_re.sql
    mysql -u root -proot ragnarok < mob_skill_db_re.sql
else
    echo "Unknow SERVER_RENEWAL_MODE MODE: $SERVER_RENEWAL_MODE"
    exit 1
fi

mysql -u root -proot ragnarok < item_db2.sql
mysql -u root -proot ragnarok < mob_db2.sql
mysql -u root -proot ragnarok < mob_skill_db2.sql
mysql -u root -proot -e "CREATE USER 'ragnarok'@'%' IDENTIFIED BY 'ragnarok'; GRANT ALL PRIVILEGES ON ragnarok.* TO 'ragnarok'@'%'; FLUSH PRIVILEGES;"

