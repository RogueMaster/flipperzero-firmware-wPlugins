rm -rf RM*-*-*.tgz RM*-*-*.zip .sconsign.dblite dist build build/f7-firmware-C/resources/apps build/f7-firmware-C/resources/nfc/RMProTrainedAmiibo
git pull
DATE_VAR=`date +%m%d`
TIME_VAR=`date +%H%M`
VER_VAR=`cat scripts/version.py | grep "or \"0" | awk -F\" '{print $4}'`
HASH_VAR=`git rev-parse \`git branch -r --sort=committerdate | tail -1\` | awk '{print substr($0,1,7)}' | tail -1`
./fbt updater_package
mv dist/f7-C/f7-update-RM420FAP "RM$DATE_VAR-$TIME_VAR"
zip -rq "RM$DATE_VAR-$TIME_VAR-$VER_VAR-$HASH_VAR.zip" "RM$DATE_VAR-$TIME_VAR"
tar -czf "RM$DATE_VAR-$TIME_VAR-$VER_VAR-$HASH_VAR.tgz" "RM$DATE_VAR-$TIME_VAR"
rm -rf "RM$DATE_VAR-$TIME_VAR"
git stash
echo "BUILD COMPLETED, ZIP AND TGZ GENERATED FOR RM$DATE_VAR-$TIME_VAR-$VER_VAR-$HASH_VAR"
