rm -rf RM*-*-*.tgz RM*-*-*.zip
# .sconsign.dblite dist build
git pull
DATE_VAR=`date +%m%d`
TIME_VAR=`date +%H%M`
HASH_VAR=`git rev-parse \`git branch -r --sort=committerdate | tail -1\` | awk '{print substr($0,1,8)}' | tail -1`
./fbt updater_package
cp -r "dist/f7-C/f7-update-rm-420-$HASH_VAR" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
zip -rq "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.zip" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
tar -czf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.tgz" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
rm -rf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
git stash
echo "BUILD COMPLETED, ZIP AND TGZ GENERATED FOR RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
