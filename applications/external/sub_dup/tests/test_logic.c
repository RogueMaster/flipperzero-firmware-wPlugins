#include "logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_duplicate_detection() {
    HashDatabase db;
    db.count = 4;

    // Setup test records
    strcpy(db.records[0].path, "file1.sub");
    db.records[0].hash = 100;
    strcpy(db.records[1].path, "file2.sub");
    db.records[1].hash = 200;
    strcpy(db.records[2].path, "file3.sub");
    db.records[2].hash = 100; // Duplicate of file1
    strcpy(db.records[3].path, "file4.sub");
    db.records[3].hash = 300;

    // Actually, hash 100 is duplicate. Sort will put 100, 100, 200, 300.

    process_duplicates(&db);

    assert(db.num_groups == 1);
    assert(db.groups[0].hash == 100);
    assert(db.groups[0].count == 2);

    printf("Test passed: Duplicate detection works.\n");
}

int main() {
    test_duplicate_detection();
    return 0;
}
