// project.c
// Full-featured Student Record System for "Coding Skills" assignment
// Features: role-based login, add/update/delete, search, sort, stats,
// attendance, CSV export, admin logging, password masking, strong pw change.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
  #include <conio.h>
  #define GETCH() _getch()
#else
  #include <termios.h>
  #include <unistd.h>
  static int GETCH() {
      struct termios oldt, newt;
      int ch;
      tcgetattr(STDIN_FILENO, &oldt);
      newt = oldt;
      newt.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &newt);
      ch = getchar();
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return ch;
  }
#endif

#define STUDENT_FILE "students.txt"
#define CREDENTIAL_FILE "credentials.txt"
#define ADMIN_LOG "admin_log.txt"
#define CSV_FILE "students.csv"

#define MAX_NAME 50
#define INIT_CAPACITY 50

typedef struct {
    int roll;
    char name[MAX_NAME];
    float marks;
    int present;    // number of times present
    int total;      // total classes
} Student;

typedef struct {
    Student *arr;
    int size;
    int capacity;
} StudentList;

char currentRole[16];
char currentUser[64];

void initList(StudentList *L) {
    L->size = 0;
    L->capacity = INIT_CAPACITY;
    L->arr = malloc(sizeof(Student) * L->capacity);
    if(!L->arr) { perror("malloc"); exit(1); }
}

void ensureCapacity(StudentList *L) {
    if(L->size >= L->capacity) {
        L->capacity *= 2;
        L->arr = realloc(L->arr, sizeof(Student) * L->capacity);
        if(!L->arr) { perror("realloc"); exit(1); }
    }
}

void freeList(StudentList *L) {
    free(L->arr);
    L->arr = NULL;
    L->size = L->capacity = 0;
}

void logAdminAction(const char *action) {
    FILE *fp = fopen(ADMIN_LOG, "a");
    if(!fp) return;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(fp, "[%s] %s: %s\n", timestr, currentUser, action);
    fclose(fp);
}

/* ------------------ Password utilities ------------------ */

void getPasswordMasked(char *buf, size_t bufsz) {
    size_t idx = 0;
    while(1) {
        int ch = GETCH();
        if(ch == '\n' || ch == '\r') {
            putchar('\n');
            break;
        } else if(ch == 127 || ch == 8) { // backspace
            if(idx > 0) {
                idx--;
                printf("\b \b");
            }
        } else if(idx + 1 < bufsz && isprint(ch)) {
            buf[idx++] = (char)ch;
            putchar('*');
        }
    }
    buf[idx] = '\0';
}

int strongPasswordCheck(const char *pw) {
    int len = strlen(pw);
    int hasUpper=0, hasLower=0, hasDigit=0, hasSpecial=0;
    if(len < 6) return 0;
    for(int i=0;i<len;i++){
        if(isupper((unsigned char)pw[i])) hasUpper=1;
        else if(islower((unsigned char)pw[i])) hasLower=1;
        else if(isdigit((unsigned char)pw[i])) hasDigit=1;
        else hasSpecial=1;
    }
    return hasUpper && hasLower && hasDigit && hasSpecial;
}

/* ------------------ File I/O for students ------------------ */

void loadStudents(StudentList *L) {
    FILE *fp = fopen(STUDENT_FILE, "r");
    if(!fp) return; // no file yet
    while(1) {
        Student s;
        int ret = fscanf(fp, "%d %49s %f %d %d", &s.roll, s.name, &s.marks, &s.present, &s.total);
        if(ret != 5) break;
        ensureCapacity(L);
        L->arr[L->size++] = s;
    }
    fclose(fp);
}

void saveStudents(StudentList *L) {
    FILE *fp = fopen(STUDENT_FILE, "w");
    if(!fp) { perror("fopen"); return; }
    for(int i=0;i<L->size;i++) {
        Student *s = &L->arr[i];
        fprintf(fp, "%d %s %.2f %d %d\n", s->roll, s->name, s->marks, s->present, s->total);
    }
    fclose(fp);
}

/* ------------------ Utility helpers ------------------ */

Student *findByRoll(StudentList *L, int roll) {
    for(int i=0;i<L->size;i++) if(L->arr[i].roll == roll) return &L->arr[i];
    return NULL;
}

int findIndexByRoll(StudentList *L, int roll) {
    for(int i=0;i<L->size;i++) if(L->arr[i].roll == roll) return i;
    return -1;
}

void printSeparator() {
    printf("-----------------------------------------------------------------\n");
}

void printHeader() {
    printSeparator();
    printf("| %-5s | %-20s | %-7s | %-10s |\n", "ROLL", "NAME", "MARKS", "ATTENDANCE");
    printSeparator();
}

void printStudentRow(const Student *s) {
    float perc = 0.0;
    if(s->total > 0) perc = ((float)s->present / s->total) * 100.0f;
    printf("| %-5d | %-20s | %7.2f | %6d/%-3d (%.0f%%) |\n",
           s->roll, s->name, s->marks, s->present, s->total, perc);
}

/* ------------------ Features ------------------ */

void addStudent(StudentList *L) {
    Student s;
    printf("\nEnter Roll Number: ");
    if(scanf("%d", &s.roll) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    if(findByRoll(L, s.roll)) {
        printf("Roll %d already exists. Aborting.\n", s.roll);
        return;
    }
    printf("Enter Name (no spaces): ");
    scanf("%49s", s.name);
    printf("Enter Marks: ");
    if(scanf("%f", &s.marks) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    s.present = 0;
    s.total = 0;
    ensureCapacity(L);
    L->arr[L->size++] = s;
    saveStudents(L);
    printf("Student added successfully.\n");
    char act[128];
    snprintf(act, sizeof(act), "Added student roll %d name %s", s.roll, s.name);
    if(strcmp(currentRole, "ADMIN") == 0) logAdminAction(act);
}

void displayStudents(StudentList *L) {
    if(L->size == 0) {
        printf("\nNo student records found.\n");
        return;
    }
    printf("\n");
    printHeader();
    for(int i=0;i<L->size;i++) printStudentRow(&L->arr[i]);
    printSeparator();
}

void updateStudent(StudentList *L) {
    int roll;
    printf("Enter roll to update: ");
    if(scanf("%d", &roll) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    Student *s = findByRoll(L, roll);
    if(!s) { printf("No student with roll %d.\n", roll); return; }
    printf("Found: %d %s %.2f\n", s->roll, s->name, s->marks);
    printf("1. Update Name\n2. Update Marks\n3. Update Both\nChoice: ");
    int ch; scanf("%d", &ch);
    if(ch == 1 || ch == 3) {
        printf("Enter new name: ");
        scanf("%49s", s->name);
    }
    if(ch == 2 || ch == 3) {
        printf("Enter new marks: ");
        scanf("%f", &s->marks);
    }
    saveStudents(L);
    printf("Student updated.\n");
    char act[128];
    snprintf(act, sizeof(act), "Updated student roll %d", roll);
    if(strcmp(currentRole, "ADMIN") == 0) logAdminAction(act);
}

void deleteStudent(StudentList *L) {
    int roll;
    printf("Enter roll to delete: ");
    if(scanf("%d", &roll) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    int idx = findIndexByRoll(L, roll);
    if(idx < 0) { printf("Roll %d not found.\n", roll); return; }
    // confirm
    printf("Are you sure to delete roll %d (y/n)? ", roll);
    char c; scanf(" %c", &c);
    if(c == 'y' || c == 'Y') {
        for(int i=idx;i<L->size-1;i++) L->arr[i] = L->arr[i+1];
        L->size--;
        saveStudents(L);
        printf("Student deleted.\n");
        char act[128];
        snprintf(act, sizeof(act), "Deleted student roll %d", roll);
        if(strcmp(currentRole, "ADMIN") == 0) logAdminAction(act);
    } else printf("Delete cancelled.\n");
}

void searchByRoll(StudentList *L) {
    int roll; printf("Enter roll to search: ");
    if(scanf("%d", &roll) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    Student *s = findByRoll(L, roll);
    if(!s) { printf("Not found.\n"); return; }
    printHeader();
    printStudentRow(s);
    printSeparator();
}

void searchByName(StudentList *L) {
    char q[64]; printf("Enter name or partial name to search: ");
    scanf("%s", q);
    int found = 0;
    printHeader();
    for(int i=0;i<L->size;i++) {
        if(strcasestr(L->arr[i].name, q)) { printStudentRow(&L->arr[i]); found = 1; }
    }
    if(!found) printf("No matches.\n");
    printSeparator();
}

/* strcasestr for portability */
char *strcasestr(const char *hay, const char *needle) {
    if(!*needle) return (char*)hay;
    for( ; *hay; hay++) {
        const char *h = hay;
        const char *n = needle;
        while(*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) { h++; n++; }
        if(!*n) return (char*)hay;
    }
    return NULL;
}

/* Sorting */
int cmpRoll(const void *a, const void *b) {
    const Student *x=a; const Student *y=b;
    return x->roll - y->roll;
}
int cmpName(const void *a, const void *b) {
    const Student *x=a; const Student *y=b;
    return strcasecmp(x->name, y->name);
}
int cmpMarksDesc(const void *a, const void *b) {
    const Student *x=a; const Student *y=b;
    if(x->marks < y->marks) return 1;
    if(x->marks > y->marks) return -1;
    return 0;
}

void sortStudents(StudentList *L) {
    printf("\nSort by:\n1. Roll (asc)\n2. Name (A-Z)\n3. Marks (high->low)\nChoice: ");
    int c; scanf("%d",&c);
    if(c==1) qsort(L->arr, L->size, sizeof(Student), cmpRoll);
    else if(c==2) qsort(L->arr, L->size, sizeof(Student), cmpName);
    else if(c==3) qsort(L->arr, L->size, sizeof(Student), cmpMarksDesc);
    else { printf("Invalid choice.\n"); return; }
    printf("Sorted successfully.\n");
}

/* Statistics */
void classStats(StudentList *L) {
    if(L->size==0) { printf("No students.\n"); return; }
    float sum=0, mx=-1e9, mn=1e9;
    int idxMax=0, idxMin=0;
    for(int i=0;i<L->size;i++){
        sum += L->arr[i].marks;
        if(L->arr[i].marks > mx) { mx = L->arr[i].marks; idxMax = i; }
        if(L->arr[i].marks < mn) { mn = L->arr[i].marks; idxMin = i; }
    }
    printf("Total students: %d\n", L->size);
    printf("Average marks: %.2f\n", sum / L->size);
    printf("Highest: Roll %d Name %s Marks %.2f\n", L->arr[idxMax].roll, L->arr[idxMax].name, L->arr[idxMax].marks);
    printf("Lowest:  Roll %d Name %s Marks %.2f\n", L->arr[idxMin].roll, L->arr[idxMin].name, L->arr[idxMin].marks);
}

/* Attendance */
void takeAttendance(StudentList *L) {
    if(L->size==0) { printf("No students to mark attendance.\n"); return; }
    printf("Mark attendance for this class.\n");
    printf("Enter present roll numbers one by one. Type 0 to finish.\n");
    // increment total for all
    for(int i=0;i<L->size;i++) L->arr[i].total += 1;
    while(1) {
        int r; printf("Present roll (0 to end): ");
        if(scanf("%d",&r) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
        if(r==0) break;
        Student *s = findByRoll(L, r);
        if(!s) printf("Roll %d not found.\n", r);
        else { s->present += 1; printf("Marked present: %d %s\n", s->roll, s->name); }
    }
    saveStudents(L);
    printf("Attendance saved.\n");
    char act[128];
    snprintf(act, sizeof(act), "Marked attendance for one class");
    if(strcmp(currentRole, "ADMIN") == 0) logAdminAction(act);
}

void exportCSV(StudentList *L) {
    FILE *fp = fopen(CSV_FILE, "w");
    if(!fp) { perror("fopen"); return; }
    fprintf(fp, "Roll,Name,Marks,Present,Total,AttendancePercent\n");
    for(int i=0;i<L->size;i++) {
        Student *s = &L->arr[i];
        float perc = 0.0f;
        if(s->total>0) perc = ((float)s->present / s->total) * 100.0f;
        fprintf(fp, "%d,%s,%.2f,%d,%d,%.0f\n",
                s->roll, s->name, s->marks, s->present, s->total, perc);
    }
    fclose(fp);
    printf("Exported to %s\n", CSV_FILE);
    char act[128]; snprintf(act, sizeof(act), "Exported data to %s", CSV_FILE);
    if(strcmp(currentRole, "ADMIN") == 0) logAdminAction(act);
}

/* Password change for current user (enforce strong password) */
void changePassword() {
    printf("Change password for %s\n", currentUser);
    char oldpw[128], newpw[128], confirm[128];
    printf("Enter old password: ");
    getPasswordMasked(oldpw, sizeof(oldpw));
    // Verify old pw by scanning credentials file
    FILE *fp = fopen(CREDENTIAL_FILE, "r");
    if(!fp) { printf("Credentials file missing.\n"); return; }
    char fuser[64], fpass[128], frole[32];
    int ok=0;
    while(fscanf(fp, "%63s %127s %31s", fuser, fpass, frole)==3) {
        if(strcmp(fuser, currentUser)==0 && strcmp(fpass, oldpw)==0) { ok=1; break; }
    }
    fclose(fp);
    if(!ok) { printf("Old password not correct.\n"); return; }
    while(1) {
        printf("Enter new password (min6, upper+lower+digit+special): ");
        getPasswordMasked(newpw, sizeof(newpw));
        if(!strongPasswordCheck(newpw)) { printf("Weak password. Try again.\n"); continue; }
        printf("Confirm new password: ");
        getPasswordMasked(confirm, sizeof(confirm));
        if(strcmp(newpw, confirm)!=0) { printf("Mismatch. Try again.\n"); continue; }
        break;
    }
    // Update credentials file: rewrite with new password
    FILE *in = fopen(CREDENTIAL_FILE, "r");
    FILE *out = fopen("temp_credentials.txt", "w");
    if(!in || !out) { perror("file"); if(in) fclose(in); if(out) fclose(out); return; }
    while(fscanf(in, "%63s %127s %31s", fuser, fpass, frole)==3) {
        if(strcmp(fuser, currentUser)==0) fprintf(out, "%s %s %s\n", fuser, newpw, frole);
        else fprintf(out, "%s %s %s\n", fuser, fpass, frole);
    }
    fclose(in); fclose(out);
    remove(CREDENTIAL_FILE);
    rename("temp_credentials.txt", CREDENTIAL_FILE);
    printf("Password changed successfully.\n");
    if(strcmp(currentRole, "ADMIN")==0) logAdminAction("Changed own password");
}

/* ------------------ Login ------------------ */

int loginSystem() {
    char username[64], password[128];
    char fileUser[64], filePass[128], fileRole[32];
    int attempts = 3;
    while(attempts--) {
        printf("Username: ");
        scanf("%63s", username);
        printf("Password: ");
        getPasswordMasked(password, sizeof(password));
        FILE *fp = fopen(CREDENTIAL_FILE, "r");
        if(!fp) { printf("Credentials file not found.\n"); return 0; }
        int found = 0;
        while(fscanf(fp, "%63s %127s %31s", fileUser, filePass, fileRole) == 3) {
            if(strcmp(username, fileUser) == 0 && strcmp(password, filePass) == 0) {
                strcpy(currentRole, fileRole);
                strcpy(currentUser, fileUser);
                found = 1; break;
            }
        }
        fclose(fp);
        if(found) return 1;
        printf("Invalid credentials. Attempts left: %d\n", attempts);
    }
    return 0;
}

/* ------------------ Menus ------------------ */

void adminMenu(StudentList *L);
void userMenu(StudentList *L);
void staffMenu(StudentList *L);
void guestMenu(StudentList *L);

void mainMenu(StudentList *L) {
    if(strcmp(currentRole, "ADMIN")==0) adminMenu(L);
    else if(strcmp(currentRole, "USER")==0) userMenu(L);
    else if(strcmp(currentRole, "STAFF")==0) staffMenu(L);
    else guestMenu(L);
}

void adminMenu(StudentList *L) {
    while(1) {
        printf("\n=== ADMIN MENU (User: %s) ===\n", currentUser);
        printf("1. Add Student\n2. View Students\n3. Update Student\n4. Delete Student\n5. Search (roll/name)\n6. Sort Students\n7. Class Statistics\n8. Take Attendance\n9. Export to CSV\n10. Change Password\n11. Logout\nChoice: ");
        int ch; if(scanf("%d",&ch)!=1) { while(getchar()!='\n'); continue; }
        switch(ch) {
            case 1: addStudent(L); break;
            case 2: displayStudents(L); break;
            case 3: updateStudent(L); break;
            case 4: deleteStudent(L); break;
            case 5:
                printf("Search by: 1.Roll 2.Name : ");
                { int t; scanf("%d",&t); if(t==1) searchByRoll(L); else searchByName(L); }
                break;
            case 6: sortStudents(L); break;
            case 7: classStats(L); break;
            case 8: takeAttendance(L); break;
            case 9: exportCSV(L); break;
            case 10: changePassword(); break;
            case 11: printf("Logging out.\n"); return;
            default: printf("Invalid choice.\n");
        }
    }
}

void userMenu(StudentList *L) {
    while(1) {
        printf("\n=== USER MENU (User: %s) ===\n", currentUser);
        printf("1. View Students\n2. Search\n3. Class Statistics\n4. Change Password\n5. Logout\nChoice: ");
        int ch; if(scanf("%d",&ch)!=1){ while(getchar()!='\n'); continue; }
        switch(ch) {
            case 1: displayStudents(L); break;
            case 2:
                printf("Search by: 1.Roll 2.Name : ");
                { int t; scanf("%d",&t); if(t==1) searchByRoll(L); else searchByName(L); }
                break;
            case 3: classStats(L); break;
            case 4: changePassword(); break;
            case 5: return;
            default: printf("Invalid.\n");
        }
    }
}

void staffMenu(StudentList *L) {
    while(1) {
        printf("\n=== STAFF MENU (User: %s) ===\n", currentUser);
        printf("1. View Students\n2. Search\n3. Take Attendance\n4. Change Password\n5. Logout\nChoice: ");
        int ch; if(scanf("%d",&ch)!=1){ while(getchar()!='\n'); continue; }
        switch(ch) {
            case 1: displayStudents(L); break;
            case 2:
                printf("Search by: 1.Roll 2.Name : ");
                { int t; scanf("%d",&t); if(t==1) searchByRoll(L); else searchByName(L); }
                break;
            case 3: takeAttendance(L); break;
            case 4: changePassword(); break;
            case 5: return;
            default: printf("Invalid.\n");
        }
    }
}

void guestMenu(StudentList *L) {
    printf("\n=== GUEST VIEW ===\n");
    displayStudents(L);
}

/* ------------------ Main ------------------ */

int main() {
    printf("==== Student Record System (Upgraded) ====\n");
    StudentList list;
    initList(&list);
    loadStudents(&list);

    if(!loginSystem()) {
        printf("Access denied. Exiting.\n");
        freeList(&list);
        return 0;
    }

    // nice welcome
    printf("Welcome %s! Role: %s\n", currentUser, currentRole);
    mainMenu(&list);

    // save list on exit
    saveStudents(&list);
    freeList(&list);
    printf("Goodbye!\n");
    return 0;
}
