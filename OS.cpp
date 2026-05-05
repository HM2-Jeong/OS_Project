#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <queue>
#include <algorithm>

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
    bool is_enqueued = false; // 큐에 들어갔는지 여부 (중복 방지용)
    
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

// 3-2. 시뮬레이션 결과 출력 함수
void printResults(const vector<Process>& processes) {
    cout << "\n=== 시뮬레이션 결과 ===\n";
    cout << "ID\tStart\tComplete\tWaiting\tTurnaround\tPromoted\n";
    
    int total_waiting = 0;
    int total_turnaround = 0;
    int promoted_count = 0;
    
    for (const auto& p : processes) {
        cout << p.id << '\t' << p.start_time << '\t' << p.completion_time 
             << '\t' << p.waiting_time << '\t' << p.turnaround_time 
             << '\t' << (p.is_promoted ? "Yes" : "No") << '\n';
        
        total_waiting += p.waiting_time;
        total_turnaround += p.turnaround_time;
        if (p.is_promoted) promoted_count++;
    }
    
    cout << "\n=== 통계 ===\n";
    cout << "평균 대기 시간: " << (double)total_waiting / processes.size() << endl;
    cout << "평균 반환 시간: " << (double)total_turnaround / processes.size() << endl;
    cout << "승격된 프로세스: " << promoted_count << endl;
}

// 4. 카운터(CPU) 구조체
struct Counter {
    int id;                 // 카운터 번호 (1~5)
    bool is_busy = false;   // 현재 처리 중인지 여부
    int remaining_time = 0; // 현재 승객의 남은 서비스 시간
    Process* current_process = nullptr; // 현재 처리 중인 승객 (포인터)
    
    // 카운터가 비었을 때 초기화하는 함수
    void clear() {
        is_busy = false;
        remaining_time = 0;
        current_process = nullptr;
    }
};

// 5. 스케줄러 클래스 (핵심 엔진)
class Scheduler {
private:
    int current_time = 0; // 시뮬레이션 전역 시간
    
    // 각 등급별 대기열 (Queue)
    // Q1은 도착 순서(FCFS)이므로 일반 queue 사용
    queue<Process*> Q1_First; 
    // Q2, Q3는 HRRN, SPN을 위해 정렬이 필요하므로 vector 사용
    vector<Process*> Q2_Business; 
    vector<Process*> Q3_Economy;  
    
    // 5개의 카운터 배열 (C1~C5)
    Counter counters[5]; 
    
    // 전체 프로세스 목록 (포인터 배열로 관리하여 원본 데이터 수정)
    vector<Process*> all_processes;
    int completed_processes = 0;

public:
    Scheduler(vector<Process>& processes) {
        // 카운터 ID 초기화 (1번부터 5번까지)
        for (int i = 0; i < 5; ++i) {
            counters[i].id = i + 1;
        }
        
        // 원본 프로세스의 주소를 저장
        for (auto& p : processes) {
            all_processes.push_back(&p);
        }
    }

    // [핵심] 시뮬레이션 메인 루프
    void run() {
        cout << "\n=== 시뮬레이션 시작 ===" << flush << endl;
        
        // 모든 프로세스가 처리될 때까지 시간 흐름
        int max_iterations = 2000; // 무한 루프 방지
        int iterations = 0;
        while (completed_processes < (int)all_processes.size() && iterations < max_iterations) {
            
            // 1. 현재 시간에 도착한 프로세스들을 각 큐에 삽입
            enqueueArrivedProcesses();
            
            // 2. [Promotion] Q3(Economy)에서 20 unit 이상 대기한 승객을 Q2로 승격
            checkPromotion();
            
            // 3. 빈 카운터에 규칙에 맞게 승객 배정 (비선점)
            assignCounters();
            
            // 4. 카운터에서 처리 중인 프로세스 시간 감소 및 완료 확인
            processCounters();
            
            // 디버그 출력 (매 50 단위 시간마다)
            if (current_time % 50 == 0) {
                cout << "Time: " << current_time << " | Completed: " << completed_processes 
                     << "/" << all_processes.size() << flush << endl;
            }
            
            // 시간 1 증가
            current_time++;
            iterations++;
        }
        
        cout << "\n최종: Time=" << current_time << " | Completed=" << completed_processes << "/" << all_processes.size() << flush << endl;
        
        if (iterations >= max_iterations) {
            cout << "경고: 최대 반복 횟수 도달. 무한 루프 감지됨.\n";
        }
        
        cout << "=== 시뮬레이션 종료 ===" << flush << endl;
    }

private:
    void enqueueArrivedProcesses() {
        for (Process* p : all_processes) {
            // 정확히 도착 시간에 한 번만 큐에 추가
            if (p->arrival_time == current_time && !p->is_enqueued) {
                p->is_enqueued = true; // 플래그 설정
                if (p->grade == FIRST) Q1_First.push(p);
                else if (p->grade == BUSINESS) Q2_Business.push_back(p);
                else if (p->grade == ECONOMY) Q3_Economy.push_back(p);
            }
        }
    }

    void checkPromotion() {
        // Economy 큐(Q3)를 순회하며 20 unit 이상 대기한 프로세스를 Business 큐(Q2)로 승격
        for (auto it = Q3_Economy.begin(); it != Q3_Economy.end(); ) {
            int waiting_time = current_time - (*it)->arrival_time;
            if (waiting_time >= 20 && !(*it)->is_promoted) {
                (*it)->is_promoted = true;
                Q2_Business.push_back(*it);
                it = Q3_Economy.erase(it); // erase는 다음 원소를 반환
            } else {
                ++it;
            }
        }
    }

  void assignCounters() {
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                Process* selected = nullptr;
                int cid = counters[i].id; // 1~5번 카운터

                // 설계서 1.3(c) 로직 정확히 구현 (코파일럿이 망친 부분 복구)
                if (cid == 1) { 
                    selected = takeFromQ1(); // C1은 First 전용 (비었으면 Idle)
                } 
                else if (cid == 2) { 
                    selected = takeFromQ2_HRRN();
                    if (selected == nullptr) selected = takeFromQ3_SPN(); // C2는 Economy 지원 허용
                } 
                else if (cid == 3) { 
                    selected = takeFromQ3_SPN(); // C3는 Economy 전용 (비었으면 Idle)
                } 
                else if (cid == 4) { 
                    selected = takeFromQ3_SPN();
                    if (selected == nullptr) selected = takeFromQ2_HRRN(); // C4는 E -> B
                } 
                else if (cid == 5) { 
                    selected = takeFromQ2_HRRN();
                    if (selected == nullptr) selected = takeFromQ3_SPN(); // C5는 B -> E
                }

                // 선택된 프로세스가 있으면 카운터에 배정하고 서비스 시작
                if (selected != nullptr) {
                    counters[i].is_busy = true;
                    counters[i].current_process = selected;
                    counters[i].remaining_time = selected->service_time;
                    selected->start_time = current_time;
                }
            }
        }
    }

    Process* takeFromQ1() {
        if (Q1_First.empty()) return nullptr;
        Process* p = Q1_First.front();
        Q1_First.pop();
        return p;
    }

    Process* takeFromQ2_HRRN() {
        if (Q2_Business.empty()) return nullptr;
        auto best_it = max_element(Q2_Business.begin(), Q2_Business.end(),
            [this](Process* a, Process* b) {
                return a->getResponseRatio(current_time) < b->getResponseRatio(current_time);
            });
        Process* p = *best_it;
        Q2_Business.erase(best_it);
        return p;
    }

    Process* takeFromQ3_SPN() {
        if (Q3_Economy.empty()) return nullptr;
        auto best_it = min_element(Q3_Economy.begin(), Q3_Economy.end(),
            [](Process* a, Process* b) {
                if (a->service_time == b->service_time) return a->arrival_time < b->arrival_time;
                return a->service_time < b->service_time;
            });
        Process* p = *best_it;
        Q3_Economy.erase(best_it);
        return p;
    }

    void processCounters() {
        for (int i = 0; i < 5; ++i) {
            if (counters[i].is_busy) {
                counters[i].remaining_time--;
                
                if (counters[i].remaining_time == 0) {
                    int completed_at = current_time + 1;
                    counters[i].current_process->completion_time = completed_at;
                    counters[i].current_process->turnaround_time = completed_at - counters[i].current_process->arrival_time;
                    counters[i].current_process->waiting_time = counters[i].current_process->turnaround_time - counters[i].current_process->service_time;
                    
                    counters[i].clear();
                    completed_processes++;
                }
            }
        }
    }
};

// 테스트용 메인 함수
int main() {
    vector<Process> processes = parseInputFile("input.txt");

    if (processes.empty()) {
        cerr << "입력 데이터가 비어 있거나 파싱에 실패했습니다." << endl;
        return 1;
    }

    cout << "입력 파싱 완료: " << processes.size() << "개 프로세스 로드" << endl;
    cout << "\n--- 로드된 프로세스 ---\n";
    printProcesses(processes);
    
    // 스케줄러 실행
    Scheduler scheduler(processes);
    scheduler.run();
    
    // 결과 출력
    printResults(processes);
    
    return 0;
}