#include "logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_duplicate_detection() {
    HashDatabase db;
    db.count = 4;

    strcpy(db.records[0].path, "file1.sub");
    db.records[0].hash = 100;
    strcpy(db.records[1].path, "file2.sub");
    db.records[1].hash = 200;
    strcpy(db.records[2].path, "file3.sub");
    db.records[2].hash = 100;
    strcpy(db.records[3].path, "file4.sub");
    db.records[3].hash = 300;

    process_duplicates(&db);

    assert(db.num_groups == 1);
    assert(db.groups[0].hash == 100);
    assert(db.groups[0].count == 2);

    printf("Test passed: Duplicate detection works.\n");
}

void test_db_remove_record() {
    HashDatabase db;
    db.count = 3;

    strcpy(db.records[0].path, "file1.sub");
    db.records[0].hash = 100;
    strcpy(db.records[1].path, "file2.sub");
    db.records[1].hash = 200;
    strcpy(db.records[2].path, "file3.sub");
    db.records[2].hash = 300;

    db_remove_record(&db, "file2.sub");

    assert(db.count == 2);
    assert(strcmp(db.records[0].path, "file1.sub") == 0);
    assert(db.records[0].hash == 100);
    assert(strcmp(db.records[1].path, "file3.sub") == 0);
    assert(db.records[1].hash == 300);

    printf("Test passed: db_remove_record works.\n");
}

void test_db_remove_record_not_found() {
    HashDatabase db;
    db.count = 2;

    strcpy(db.records[0].path, "file1.sub");
    db.records[0].hash = 100;
    strcpy(db.records[1].path, "file2.sub");
    db.records[1].hash = 200;

    db_remove_record(&db, "nonexistent.sub");

    assert(db.count == 2);
    assert(strcmp(db.records[0].path, "file1.sub") == 0);
    assert(strcmp(db.records[1].path, "file2.sub") == 0);

    printf("Test passed: db_remove_record handles missing file.\n");
}

void test_db_remove_record_last() {
    HashDatabase db;
    db.count = 1;

    strcpy(db.records[0].path, "only.sub");
    db.records[0].hash = 100;

    db_remove_record(&db, "only.sub");

    assert(db.count == 0);

    printf("Test passed: db_remove_record removes last record.\n");
}

int main() {
    test_duplicate_detection();
    test_db_remove_record();
    test_db_remove_record_not_found();
    test_db_remove_record_last();
    return 0;
}
