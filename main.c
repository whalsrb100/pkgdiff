#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 512
#define MAX_PACKAGES 10000
#define MAX_NAME_LENGTH 256
#define MAX_VERSION_LENGTH 128
#define MAX_ARCH_LENGTH 32

typedef struct {
    char package_a[MAX_LINE_LENGTH];
    char package_b[MAX_LINE_LENGTH];
    char status;
    char sort_key[MAX_NAME_LENGTH]; // 정렬용 키 (패키지 이름)
} ComparisonResult;

typedef struct {
    char name[MAX_NAME_LENGTH];
    char version[MAX_VERSION_LENGTH];
    char arch[MAX_ARCH_LENGTH];
    char full_line[MAX_LINE_LENGTH];
} Package;

// 패키지 문자열을 파싱하여 이름, 버전, 아키텍처로 분리
// RPM 패키지 형식: name-version-release.arch
int parse_package(const char* package_str, Package* pkg) {
    char temp[MAX_LINE_LENGTH];
    strncpy(temp, package_str, MAX_LINE_LENGTH - 1);
    temp[MAX_LINE_LENGTH - 1] = '\0';
    
    // 줄바꿈 문자 제거
    char* newline = strchr(temp, '\n');
    if (newline) *newline = '\0';
    
    // 전체 라인 저장
    strncpy(pkg->full_line, temp, MAX_LINE_LENGTH - 1);
    pkg->full_line[MAX_LINE_LENGTH - 1] = '\0';
    
    // 마지막 점(.)을 찾아서 아키텍처 분리
    char* last_dot = strrchr(temp, '.');
    if (!last_dot) return 0;
    
    // 아키텍처 추출
    strncpy(pkg->arch, last_dot + 1, MAX_ARCH_LENGTH - 1);
    pkg->arch[MAX_ARCH_LENGTH - 1] = '\0';
    *last_dot = '\0';
    
    // 이제 temp에는 "name-version-release" 형태가 남음
    // 마지막 두 개의 하이픈을 찾아야 함 (version-release 부분)
    char* last_hyphen = strrchr(temp, '-');
    if (!last_hyphen) return 0;
    
    char* second_last_hyphen = NULL;
    char* ptr = temp;
    while (ptr < last_hyphen) {
        char* next_hyphen = strchr(ptr, '-');
        if (next_hyphen && next_hyphen < last_hyphen) {
            second_last_hyphen = next_hyphen;
            ptr = next_hyphen + 1;
        } else {
            break;
        }
    }
    
    if (!second_last_hyphen) return 0;
    
    // version-release 추출 (second_last_hyphen + 1 부터 끝까지)
    strncpy(pkg->version, second_last_hyphen + 1, MAX_VERSION_LENGTH - 1);
    pkg->version[MAX_VERSION_LENGTH - 1] = '\0';
    *second_last_hyphen = '\0';
    
    // 나머지는 패키지 이름
    strncpy(pkg->name, temp, MAX_NAME_LENGTH - 1);
    pkg->name[MAX_NAME_LENGTH - 1] = '\0';
    
    return 1;
}

// 파일에서 패키지 목록을 읽어오기
int read_packages(const char* filename, Package packages[], int max_count) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }
    
    char line[MAX_LINE_LENGTH];
    int count = 0;
    
    while (fgets(line, sizeof(line), file) && count < max_count) {
        // 빈 줄 건너뛰기
        if (strlen(line) <= 1) continue;
        
        if (parse_package(line, &packages[count])) {
            count++;
        }
    }
    
    fclose(file);
    return count;
}

// 두 패키지가 완전히 동일한지 확인 (이름, 버전, 아키텍처 모두)
int packages_equal(const Package* a, const Package* b) {
    return strcmp(a->name, b->name) == 0 && 
           strcmp(a->version, b->version) == 0 && 
           strcmp(a->arch, b->arch) == 0;
}

// 비교 결과 정렬을 위한 함수
int compare_results(const void* a, const void* b) {
    const ComparisonResult* result_a = (const ComparisonResult*)a;
    const ComparisonResult* result_b = (const ComparisonResult*)b;
    return strcmp(result_a->sort_key, result_b->sort_key);
}

// 정확히 일치하는 패키지 검색 (이름+버전+아키텍처)
int find_exact_package(const Package packages[], int count, const Package* target, const int processed[]) {
    for (int i = 0; i < count; i++) {
        if (!processed[i] && packages_equal(&packages[i], target)) {
            return i;
        }
    }
    return -1;
}

// 패키지 이름으로 검색 (아직 처리되지 않은 패키지만)
int find_package_by_name(const Package packages[], int count, const char* name, const int processed[]) {
    for (int i = 0; i < count; i++) {
        if (!processed[i] && strcmp(packages[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    int xlsx_output = 0;
    char* file_a = NULL;
    char* file_b = NULL;
    char* output_file = "rpm_diff_result.csv";
    
    // 옵션 파싱
    if (argc < 3 || argc > 5) {
        fprintf(stderr, "Usage: %s [--xlsx [output.csv]] <package_list_A> <package_list_B>\n", argv[0]);
        fprintf(stderr, "       %s <package_list_A> <package_list_B>\n", argv[0]);
        return 1;
    }
    
    int arg_idx = 1;
    
    // --xlsx 옵션 확인
    if (argc >= 4 && strcmp(argv[1], "--xlsx") == 0) {
        xlsx_output = 1;
        arg_idx = 2;
        
        // 사용자 지정 출력 파일명이 있는지 확인
        if (argc == 5) {
            output_file = argv[2];
            arg_idx = 3;
        }
    }
    
    file_a = argv[arg_idx];
    file_b = argv[arg_idx + 1];
    
    Package* packages_a = malloc(MAX_PACKAGES * sizeof(Package));
    Package* packages_b = malloc(MAX_PACKAGES * sizeof(Package));
    
    if (!packages_a || !packages_b) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(packages_a);
        free(packages_b);
        return 1;
    }
    
    int count_a = read_packages(file_a, packages_a, MAX_PACKAGES);
    int count_b = read_packages(file_b, packages_b, MAX_PACKAGES);
    
    if (count_a < 0) {
        fprintf(stderr, "Error: Failed to read packages from %s\n", file_a);
        return 1;
    }
    
    if (count_b < 0) {
        fprintf(stderr, "Error: Failed to read packages from %s\n", file_b);
        return 1;
    }
    
    printf("Loaded %d packages from %s\n", count_a, file_a);
    printf("Loaded %d packages from %s\n", count_b, file_b);
    if (!xlsx_output) {
        printf("\nComparison results:\n");
        printf("Format: A_package\\tstatus\\tB_package\n");
        printf("Status: < (A only), > (B only), | (different version/arch), = (identical)\n\n");
    }
    
    // A 패키지들을 이름순으로 정렬
    ComparisonResult* results = malloc((count_a + count_b) * sizeof(ComparisonResult));
    int result_count = 0;
    
    // A에 있는 패키지들을 B와 비교
    int* processed_b = calloc(count_b, sizeof(int));
    
    for (int i = 0; i < count_a; i++) {
        // 먼저 정확히 일치하는 패키지 검색
        int found_idx = find_exact_package(packages_b, count_b, &packages_a[i], processed_b);
        
        if (found_idx != -1) {
            // 완전히 동일한 패키지 발견
            processed_b[found_idx] = 1;
            strcpy(results[result_count].package_a, packages_a[i].full_line);
            strcpy(results[result_count].package_b, packages_b[found_idx].full_line);
            results[result_count].status = '=';
            strcpy(results[result_count].sort_key, packages_a[i].name);
            result_count++;
        } else {
            // 정확히 일치하는 패키지가 없으면 이름만 일치하는 패키지 검색
            found_idx = find_package_by_name(packages_b, count_b, packages_a[i].name, processed_b);
            
            if (found_idx == -1) {
                // A에만 있는 패키지
                strcpy(results[result_count].package_a, packages_a[i].full_line);
                strcpy(results[result_count].package_b, "");
                results[result_count].status = '<';
                strcpy(results[result_count].sort_key, packages_a[i].name);
                result_count++;
            } else {
                // 동일한 이름이지만 버전/아키텍처가 다른 패키지
                processed_b[found_idx] = 1;
                strcpy(results[result_count].package_a, packages_a[i].full_line);
                strcpy(results[result_count].package_b, packages_b[found_idx].full_line);
                results[result_count].status = '|';
                strcpy(results[result_count].sort_key, packages_a[i].name);
                result_count++;
            }
        }
    }
    
    // B에만 있는 패키지들 추가
    for (int i = 0; i < count_b; i++) {
        if (!processed_b[i]) {
            strcpy(results[result_count].package_a, "");
            strcpy(results[result_count].package_b, packages_b[i].full_line);
            results[result_count].status = '>';
            strcpy(results[result_count].sort_key, packages_b[i].name);
            result_count++;
        }
    }
    
    // A 관련 결과와 B에만 있는 결과를 분리
    ComparisonResult* a_results = malloc(count_a * sizeof(ComparisonResult));
    ComparisonResult* b_only_results = malloc(count_b * sizeof(ComparisonResult));
    int a_count = 0, b_only_count = 0;
    
    // 결과를 A 관련과 B에만 있는 것으로 분리
    for (int i = 0; i < result_count; i++) {
        if (results[i].status == '>') {
            b_only_results[b_only_count++] = results[i];
        } else {
            a_results[a_count++] = results[i];
        }
    }
    
    // A 관련 결과를 A 패키지 이름순으로 정렬
    qsort(a_results, a_count, sizeof(ComparisonResult), compare_results);
    
    // B에만 있는 결과를 B 패키지 이름순으로 정렬
    qsort(b_only_results, b_only_count, sizeof(ComparisonResult), compare_results);
    
    // 출력 방식 결정
    if (xlsx_output) {
        // CSV 파일로 출력
        FILE* csv_file = fopen(output_file, "w");
        if (!csv_file) {
            fprintf(stderr, "Error: Cannot create output file %s\n", output_file);
            free(a_results);
            free(b_only_results);
            free(results);
            free(processed_b);
            free(packages_a);
            free(packages_b);
            return 1;
        }
        
        // CSV 헤더 작성
        fprintf(csv_file, "Package A,Status,Package B\n");
        
        // A 관련 결과 출력
        for (int i = 0; i < a_count; i++) {
            if (a_results[i].status == '=') {
                fprintf(csv_file, "\"%s\",\"=\",\"%s\"\n", 
                       a_results[i].package_a, a_results[i].package_b);
            } else if (a_results[i].status == '|') {
                fprintf(csv_file, "\"%s\",\"|\",\"%s\"\n", 
                       a_results[i].package_a, a_results[i].package_b);
            } else if (a_results[i].status == '<') {
                fprintf(csv_file, "\"%s\",\"<\",\"\"\n", 
                       a_results[i].package_a);
            }
        }
        
        // B에만 있는 결과 출력
        for (int i = 0; i < b_only_count; i++) {
            fprintf(csv_file, "\"\",\">\",\"%s\"\n", 
                   b_only_results[i].package_b);
        }
        
        fclose(csv_file);
        printf("Results saved to %s\n", output_file);
    } else {
        // 콘솔 출력
        // A 관련 결과 출력
        for (int i = 0; i < a_count; i++) {
            if (a_results[i].status == '=') {
                printf("%s\t=\t%s\n", a_results[i].package_a, a_results[i].package_b);
            } else if (a_results[i].status == '|') {
                printf("%s\t|\t%s\n", a_results[i].package_a, a_results[i].package_b);
            } else if (a_results[i].status == '<') {
                printf("%s\t<\t\n", a_results[i].package_a);
            }
        }
        
        // B에만 있는 결과 출력
        for (int i = 0; i < b_only_count; i++) {
            printf("\t>\t%s\n", b_only_results[i].package_b);
        }
    }
    
    free(a_results);
    free(b_only_results);
    
    free(results);
    
    free(processed_b);
    free(packages_a);
    free(packages_b);
    return 0;
}
