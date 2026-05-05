#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

// 가독성을 위한 등급 열거형 (enum)
enum Grade {
    FIRST = 1,
    BUSINESS = 2,
    ECONOMY = 3
};

// 1. 프로세스 데이터 구조체
struct Process {
    int id;             // 프로세스 ID
    Grade grade;        // 등급 (1: First, 2: Business, 3: Economy)
    int arrival_time;   // 도착 시간
    int service_time;   // 서비스 시간
    
    // 시뮬레이션 과정에서 기록할 통계용 변수들
    int start_time = -1;      // 서비스 시작 시간
    int completion_time = 0;  // 서비스 완료 시간
    int waiting_time = 0;     // 최종 대기 시간
    int turnaround_time = 0;  // 최종 반환 시간 (TT)
    
    bool is_promoted = false; // Economy -> Business 승격 여부
    
    // [핵심] HRRN 계산용 응답률(Response Ratio) 반환 함수
    // 공식: (대기시간 + 서비스시간) / 서비스시간
    // 주의: Promotion 되더라도 arrival_time을 그대로 쓰기 때문에 우리가 정한 '방법 A'가 자동 적용됨.
    double getResponseRatio(int current_time) const {
        int current_wait = current_time - arrival_time;
        return static_cast<double>(current_wait + service_time) / service_time;
    }
};

// 2. input.txt 파일을 읽어서 Process 벡터로 반환하는 함수
// 입력 형식: ID 도착시간 등급 서비스시간
vector<Process> parseInputFile(const string& filename) {
    vector<Process> processes;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        cerr << "에러: " << filename << " 파일을 열 수 없습니다!" << endl;
        return processes;
    }

    // 예시 데이터 포맷: 1 0 3 7
    // 의미: ID=1, arrival_time=0, grade=3, service_time=7
    while (getline(file, line)) {
        if (line.empty()) continue;
        
        stringstream ss(line);
        Process p;
        int grade_int;
        
        // 띄어쓰기 또는 탭 기준으로 파싱
        if (!(ss >> p.id >> p.arrival_time >> grade_int >> p.service_time)) {
            cerr << "경고: 잘못된 입력 행을 건너뜀 -> " << line << endl;
            continue;
        }
        p.grade = static_cast<Grade>(grade_int);
        
        processes.push_back(p);
    }

    file.close();
    return processes;
}

// 3. 읽은 데이터를 확인하는 출력 함수
void printProcesses(const vector<Process>& processes) {
    cout << "ID\tArrival\tGrade\tService\n";
    for (const auto& p : processes) {
        cout << p.id << '\t' << p.arrival_time << '\t' << static_cast<int>(p.grade)
             << '\t' << p.service_time << '\n';
    }
}

// 테스트용 메인 함수
int main() {
    vector<Process> processes = parseInputFile("input.txt");

    if (processes.empty()) {
        cerr << "입력 데이터가 비어 있거나 파싱에 실패했습니다." << endl;
        return 1;
    }

    cout << "입력 파싱 완료: " << processes.size() << "개 프로세스 로드" << endl;
    printProcesses(processes);
    return 0;
}