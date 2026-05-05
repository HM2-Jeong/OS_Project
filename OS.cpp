#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <queue>
#include <algorithm>
#include <iomanip>

using namespace std;

// 가독성을 위한 등급 열거형
enum Grade {
    FIRST = 1,
    BUSINESS = 2,
    ECONOMY = 3
};

// 1. 프로세스 데이터 구조체
struct Process {
    int id;
    Grade grade;
    int arrival_time;
    int service_time;
    
    int start_time = -1;
    int completion_time = 0;
    int waiting_time = 0;
    int turnaround_time = 0;
    
    bool is_promoted = false;
    bool is_enqueued = false;
    
    // HRRN 계산
    double getResponseRatio(int current_time) const {
        int current_wait = current_time - arrival_time;
        return static_cast<double>(current_wait + service_time) / service_time;
    }
};

// 2. 파일 파싱 함수
vector<Process> parseInputFile(const string& filename) {
    vector<Process> processes;
    ifstream file(filename);
    string line;
    if (!file.is_open()) {
        cerr << "에러: " << filename << " 파일을 열 수 없습니다!" << endl;
        return processes;
    }
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        Process p;
        int grade_int;
        if (!(ss >> p.id >> p.arrival_time >> grade_int >> p.service_time)) continue;
        p.grade = static_cast<Grade>(grade_int);
        processes.push_back(p);
    }
    file.close();
    return processes;
}

// [추가 1] Baseline 연속 실행을 위한 데이터 리셋 함수
void resetProcesses(vector<Process>& processes) {
    for (auto& p : processes) {
        p.start_time = -1;
        p.completion_time = 0;
        p.waiting_time = 0;
        p.turnaround_time = 0;
        p.is_promoted = false;
        p.is_enqueued = false;
    }
}

// 3. 카운터 구조체
struct Counter {
    int id;
    bool is_busy = false;
    int remaining_time = 0;
    Process* current_process = nullptr;
    
    void clear() {
        is_busy = false;
        remaining_time = 0;
        current_process = nullptr;
    }
};

// =====================================================================
// 4. [Strategy Pattern] 스케줄러 인터페이스
// =====================================================================
class ISchedulerStrategy {
public:
    virtual ~ISchedulerStrategy() = default;
    virtual void assignCounters(Counter counters[], int current_time, 
                                queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) = 0;
    virtual string getStrategyName() = 0;
    
    // [추가 2] Promotion 사용 여부를 자식 클래스가 결정하도록 가상 함수 추가 (기본값 false)
    virtual bool usePromotion() { return false; } 
};

// =====================================================================
// 5. 우리의 제안 알고리즘 전략 (Proposed Strategy)
// =====================================================================
class ProposedStrategy : public ISchedulerStrategy {
public:
    string getStrategyName() override { return "Hybrid MLFQ (Proposed)"; }
    
    // 제안 스케줄러는 Promotion을 사용하므로 true 반환!
    bool usePromotion() override { return true; }

    void assignCounters(Counter counters[], int current_time, 
                        queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) override {
        
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                Process* selected = nullptr;
                int cid = counters[i].id;

                if (cid == 1) { 
                    selected = takeFromQ1(q1); 
                } 
                else if (cid == 2) { 
                    selected = takeFromQ2_HRRN(q2, current_time); // Business 전용
                } 
                else if (cid == 3) { 
                    selected = takeFromQ3_SPN(q3); // Economy 전용
                } 
                else if (cid == 4) { 
                    selected = takeFromQ3_SPN(q3);
                    if (!selected) selected = takeFromQ2_HRRN(q2, current_time); 
                } 
                else if (cid == 5) { 
                    selected = takeFromQ2_HRRN(q2, current_time);
                    if (!selected) selected = takeFromQ3_SPN(q3); 
                }

                if (selected != nullptr) {
                    counters[i].is_busy = true;
                    counters[i].current_process = selected;
                    counters[i].remaining_time = selected->service_time;
                    selected->start_time = current_time;
                }
            }
        }
    }

private:
    Process* takeFromQ1(queue<Process*>& q) {
        if (q.empty()) return nullptr;
        Process* p = q.front(); q.pop();
        return p;
    }

    Process* takeFromQ2_HRRN(vector<Process*>& q, int current_time) {
        if (q.empty()) return nullptr;
        auto best = max_element(q.begin(), q.end(),
            [current_time](Process* a, Process* b) {
                return a->getResponseRatio(current_time) < b->getResponseRatio(current_time);
            });
        Process* p = *best; q.erase(best);
        return p;
    }

    Process* takeFromQ3_SPN(vector<Process*>& q) {
        if (q.empty()) return nullptr;
        auto best = min_element(q.begin(), q.end(),
            [](Process* a, Process* b) {
                if (a->service_time == b->service_time) return a->arrival_time < b->arrival_time;
                return a->service_time < b->service_time;
            });
        Process* p = *best; q.erase(best);
        return p;
    }
};

// =====================================================================
// 6. 스케줄러 핵심 엔진 (Context)
// =====================================================================
class Scheduler {
private:
    int current_time = 0;
    queue<Process*> Q1_First; 
    vector<Process*> Q2_Business; 
    vector<Process*> Q3_Economy;  
    Counter counters[5]; 
    vector<Process*> all_processes;
    int completed_processes = 0;
    
    ISchedulerStrategy* strategy;

public:
    Scheduler(vector<Process>& processes, ISchedulerStrategy* strat) : strategy(strat) {
        for (int i = 0; i < 5; ++i) counters[i].id = i + 1;
        for (auto& p : processes) all_processes.push_back(&p);
    }

    void run() {
        cout << "\n=== 시뮬레이션 시작 (" << strategy->getStrategyName() << ") ===\n";
        int max_iterations = 3000; 
        int iterations = 0;
        
        while (completed_processes < (int)all_processes.size() && iterations < max_iterations) {
            enqueueArrivedProcesses();
            
            // [수정 3] 현재 전략이 Promotion을 허용할 때만 실행! (Phase 3 대비 완벽 처리)
            if (strategy->usePromotion()) {
                checkPromotion();
            }
            
            strategy->assignCounters(counters, current_time, Q1_First, Q2_Business, Q3_Economy);
            processCounters();
            
            current_time++;
            iterations++;
        }
        cout << "=== 시뮬레이션 종료 (최종 Time: " << current_time << ") ===\n";
    }

private:
    void enqueueArrivedProcesses() {
        for (Process* p : all_processes) {
            if (p->arrival_time == current_time && !p->is_enqueued) {
                p->is_enqueued = true;
                if (p->grade == FIRST) Q1_First.push(p);
                else if (p->grade == BUSINESS) Q2_Business.push_back(p);
                else if (p->grade == ECONOMY) Q3_Economy.push_back(p);
            }
        }
    }

    void checkPromotion() {
        for (auto it = Q3_Economy.begin(); it != Q3_Economy.end(); ) {
            int waiting_time = current_time - (*it)->arrival_time;
            if (waiting_time > 20 && !(*it)->is_promoted) {
                (*it)->is_promoted = true;
                Q2_Business.push_back(*it);
                it = Q3_Economy.erase(it); 
            } else {
                ++it;
            }
        }
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

// =====================================================================
// 7. 결과 출력 (개별 승객 및 등급별 통계)
// =====================================================================
void printResults(const vector<Process>& processes) {
    cout << "\n=== 시뮬레이션 결과 ===\n";
    
    // [추가 4] 개별 승객 결과 테이블 출력
    cout << "ID\tArr\tSrv\tGrade\tWait\tTurn\tPromoted\n";
    cout << "--------------------------------------------------------\n";
    for (const auto& p : processes) {
        cout << p.id << "\t" << p.arrival_time << "\t" << p.service_time << "\t" 
             << p.grade << "\t" << p.waiting_time << "\t" << p.turnaround_time << "\t" 
             << (p.is_promoted ? "Yes" : "No") << "\n";
    }
    
    int t_wait[4] = {0}, t_turn[4] = {0}, count[4] = {0};
    int promoted_count = 0;
    
    for (const auto& p : processes) {
        int g = p.grade;
        t_wait[g] += p.waiting_time;
        t_turn[g] += p.turnaround_time;
        count[g]++;
        
        t_wait[0] += p.waiting_time;
        t_turn[0] += p.turnaround_time;
        count[0]++;
        
        if (p.is_promoted) promoted_count++;
    }
    
    cout << "\n[ 등급별 평균 반환 시간 (ATT) 및 대기 시간 ]\n";
    cout << fixed << setprecision(2);
    if(count[1] > 0) cout << " - First Class : ATT = " << (double)t_turn[1]/count[1] << " / Wait = " << (double)t_wait[1]/count[1] << " (" << count[1] << "명)\n";
    if(count[2] > 0) cout << " - Business    : ATT = " << (double)t_turn[2]/count[2] << " / Wait = " << (double)t_wait[2]/count[2] << " (" << count[2] << "명)\n";
    if(count[3] > 0) cout << " - Economy     : ATT = " << (double)t_turn[3]/count[3] << " / Wait = " << (double)t_wait[3]/count[3] << " (" << count[3] << "명)\n";
    
    cout << "\n[ 전체 시스템 통계 ]\n";
    cout << " - 전체 평균 ATT : " << (double)t_turn[0]/count[0] << "\n";
    cout << " - 전체 평균 대기시간: " << (double)t_wait[0]/count[0] << "\n";
    cout << " - 승격된 프로세스 수: " << promoted_count << "명\n";
}

// 8. 메인 함수
int main() {
    vector<Process> processes = parseInputFile("input.txt");
    if (processes.empty()) return 1;

    // --- 실행 1: 우리가 제안한 알고리즘 ---
    ProposedStrategy strategy1;
    Scheduler scheduler1(processes, &strategy1);
    scheduler1.run();
    printResults(processes);
    
    // (이후 Phase 3 구현 시 아래처럼 리셋 후 다른 전략을 실행하면 됩니다)
    // resetProcesses(processes);
    // BaselineStrategy strategy2;
    // Scheduler scheduler2(processes, &strategy2);
    // scheduler2.run();
    // printResults(processes);

    return 0;
}