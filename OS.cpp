#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <queue>
#include <algorithm>
#include <iomanip>

using namespace std;

enum Grade { FIRST = 1, BUSINESS = 2, ECONOMY = 3 };

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
    
    double getResponseRatio(int current_time) const {
        int current_wait = current_time - arrival_time;
        return static_cast<double>(current_wait + service_time) / service_time;
    }
};

vector<Process> parseInputFile(const string& filename) {
    vector<Process> processes;
    ifstream file(filename);
    string line;
    if (!file.is_open()) { cerr << "에러: 파일 없음\n"; return processes; }
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        Process p;
        int grade_int;
        if (!(ss >> p.id >> p.arrival_time >> grade_int >> p.service_time)) continue;
        p.grade = static_cast<Grade>(grade_int);
        processes.push_back(p);
    }
    return processes;
}

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

struct Counter {
    int id;
    bool is_busy = false;
    int remaining_time = 0;
    Process* current_process = nullptr;
    int passengers_served = 0;
    int total_service_time = 0;
    void clear() { is_busy = false; remaining_time = 0; current_process = nullptr; }
    void resetStats() { passengers_served = 0; total_service_time = 0; }
};

class ISchedulerStrategy {
public:
    virtual ~ISchedulerStrategy() = default;
    virtual void assignCounters(Counter counters[], int current_time,
                                queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) = 0;
    virtual string getStrategyName() = 0;
    virtual bool usePromotion() { return false; }
};

class ProposedStrategy : public ISchedulerStrategy {
public:
    string getStrategyName() override { return "Hybrid MLFQ (Proposed)"; }
    bool usePromotion() override { return true; }

    void assignCounters(Counter counters[], int current_time,
                        queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) override {
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                Process* selected = nullptr;
                int cid = counters[i].id;
                if      (cid == 1) selected = takeFromQ1(q1);
                else if (cid == 2) selected = takeFromQ2_HRRN(q2, current_time);
                else if (cid == 3) selected = takeFromQ3_SPN(q3);
                else if (cid == 4) { selected = takeFromQ3_SPN(q3); if (!selected) selected = takeFromQ2_HRRN(q2, current_time); }
                else if (cid == 5) { selected = takeFromQ2_HRRN(q2, current_time); if (!selected) selected = takeFromQ3_SPN(q3); }
                if (selected) { counters[i].is_busy = true; counters[i].current_process = selected; counters[i].remaining_time = selected->service_time; selected->start_time = current_time; }
            }
        }
    }

private:
    Process* takeFromQ1(queue<Process*>& q) {
        if (q.empty()) return nullptr;
        Process* p = q.front(); q.pop(); return p;
    }
    Process* takeFromQ2_HRRN(vector<Process*>& q, int current_time) {
        if (q.empty()) return nullptr;
        auto best = max_element(q.begin(), q.end(),
            [current_time](Process* a, Process* b) { return a->getResponseRatio(current_time) < b->getResponseRatio(current_time); });
        Process* p = *best; q.erase(best); return p;  // 버그 수정: 최고 -> best
    }
    Process* takeFromQ3_SPN(vector<Process*>& q) {
        if (q.empty()) return nullptr;
        auto best = min_element(q.begin(), q.end(),
            [](Process* a, Process* b) {
                if (a->service_time == b->service_time) return a->arrival_time < b->arrival_time;
                return a->service_time < b->service_time;
            });
        Process* p = *best; q.erase(best); return p;  // 버그 수정: 최고 -> best
    }
};

class BaselineA_FCFS : public ISchedulerStrategy {
public:
    string getStrategyName() override { return "Baseline A (Strict FCFS)"; }
    void assignCounters(Counter counters[], int current_time,
                        queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) override {
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                Process* selected = nullptr;
                vector<Process*>* selected_q = nullptr;
                if (!q1.empty()) { selected = q1.front(); selected_q = nullptr; }
                if (!q2.empty() && (selected == nullptr || q2.front()->arrival_time < selected->arrival_time)) { selected = q2.front(); selected_q = &q2; }
                if (!q3.empty() && (selected == nullptr || q3.front()->arrival_time < selected->arrival_time)) { selected = q3.front(); selected_q = &q3; }
                if (selected) {
                    counters[i].is_busy = true; counters[i].current_process = selected;
                    counters[i].remaining_time = selected->service_time; selected->start_time = current_time;
                    if (selected_q == &q2) q2.erase(q2.begin());
                    else if (selected_q == &q3) q3.erase(q3.begin());
                    else q1.pop();
                }
            }
        }
    }
};

class BaselineB_Priority : public ISchedulerStrategy {
public:
    string getStrategyName() override { return "Baseline B (Strict Priority FCFS)"; }
    void assignCounters(Counter counters[], int current_time,
                        queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) override {
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                Process* selected = nullptr;
                if (!q1.empty()) { selected = q1.front(); q1.pop(); }
                else if (!q2.empty()) { selected = q2.front(); q2.erase(q2.begin()); }
                else if (!q3.empty()) { selected = q3.front(); q3.erase(q3.begin()); }
                if (selected) { counters[i].is_busy = true; counters[i].current_process = selected; counters[i].remaining_time = selected->service_time; selected->start_time = current_time; }
            }
        }
    }
};

class BaselineC_SJF : public ISchedulerStrategy {
public:
    string getStrategyName() override { return "Baseline C (Non-preemptive SJF)"; }
    void assignCounters(Counter counters[], int current_time,
                        queue<Process*>& q1, vector<Process*>& q2, vector<Process*>& q3) override {
        for (int i = 0; i < 5; ++i) {
            if (!counters[i].is_busy) {
                vector<Process*> waiting;
                queue<Process*> q1_copy = q1;
                while (!q1_copy.empty()) { waiting.push_back(q1_copy.front()); q1_copy.pop(); }
                waiting.insert(waiting.end(), q2.begin(), q2.end());
                waiting.insert(waiting.end(), q3.begin(), q3.end());
                if (waiting.empty()) continue;
                auto best = min_element(waiting.begin(), waiting.end(),
                    [](Process* a, Process* b) {
                        if (a->service_time == b->service_time) return a->arrival_time < b->arrival_time;
                        return a->service_time < b->service_time;
                    });
                Process* selected = *best;
                bool in_q1 = false;
                queue<Process*> q1_scan = q1;
                while (!q1_scan.empty()) { if (q1_scan.front() == selected) { in_q1 = true; break; } q1_scan.pop(); }
                if (in_q1) {
                    queue<Process*> rebuilt;
                    while (!q1.empty()) { Process* cur = q1.front(); q1.pop(); if (cur != selected) rebuilt.push(cur); }
                    q1 = rebuilt;
                } else {
                    auto it2 = find(q2.begin(), q2.end(), selected);
                    if (it2 != q2.end()) q2.erase(it2);
                    else { auto it3 = find(q3.begin(), q3.end(), selected); if (it3 != q3.end()) q3.erase(it3); }
                }
                counters[i].is_busy = true; counters[i].current_process = selected;
                counters[i].remaining_time = selected->service_time; selected->start_time = current_time;
            }
        }
    }
};

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
        for (int i = 0; i < 5; ++i) counters[i].resetStats();
        int max_iterations = 3000, iterations = 0;
        while (completed_processes < (int)all_processes.size() && iterations < max_iterations) {
            enqueueArrivedProcesses();
            if (strategy->usePromotion()) checkPromotion();
            strategy->assignCounters(counters, current_time, Q1_First, Q2_Business, Q3_Economy);
            processCounters();
            current_time++; iterations++;
        }
        cout << "=== 시뮬레이션 종료 (최종 Time: " << current_time << ") ===\n";
    }

    void printCounterStats() {
        string types[] = {"", "First 전용", "Business 전용", "Economy 전용", "Flex (E→B)", "Flex (B→E)"};
        cout << "\n[ 카운터별 통계 ]\n";
        cout << left << setw(6) << "카운터" << setw(16) << "유형"
             << setw(12) << "처리승객" << setw(12) << "처리시간" << "유휴시간\n";
        cout << string(56, '-') << "\n";
        int ts = 0, ti = 0;
        for (int i = 0; i < 5; ++i) {
            int idle = current_time - counters[i].total_service_time;
            cout << left << setw(6) << ("C" + to_string(i+1))
                 << setw(16) << types[i+1]
                 << setw(12) << counters[i].passengers_served
                 << setw(12) << counters[i].total_service_time
                 << idle << "\n";
            ts += counters[i].total_service_time;
            ti += idle;
        }
        cout << string(56, '-') << "\n";
        cout << " 전체 서비스=" << ts << "  유휴=" << ti
             << "  가동률=" << fixed << setprecision(1)
             << (double)ts / (5 * current_time) * 100 << "%\n";
    }

private:
    void enqueueArrivedProcesses() {
        for (Process* p : all_processes) {
            if (p->arrival_time == current_time && !p->is_enqueued) {
                p->is_enqueued = true;
                if (p->grade == FIRST) Q1_First.push(p);
                else if (p->grade == BUSINESS) Q2_Business.push_back(p);
                else Q3_Economy.push_back(p);
            }
        }
    }
    void checkPromotion() {
        for (auto it = Q3_Economy.begin(); it != Q3_Economy.end(); ) {
            int wait = current_time - (*it)->arrival_time;
            if (wait > 20 && !(*it)->is_promoted) { (*it)->is_promoted = true; Q2_Business.push_back(*it); it = Q3_Economy.erase(it); }
            else ++it;
        }
    }
    void processCounters() {
        for (int i = 0; i < 5; ++i) {
            if (counters[i].is_busy) {
                counters[i].remaining_time--;
                if (counters[i].remaining_time == 0) {
                    int done = current_time + 1;
                    counters[i].current_process->completion_time = done;
                    counters[i].current_process->turnaround_time = done - counters[i].current_process->arrival_time;
                    counters[i].current_process->waiting_time = counters[i].current_process->turnaround_time - counters[i].current_process->service_time;
                    counters[i].passengers_served++;
                    counters[i].total_service_time += counters[i].current_process->service_time;
                    counters[i].clear(); completed_processes++;
                }
            }
        }
    }
};

void printResults(const vector<Process>& processes, const string& title = "") {
    cout << "\n=== 시뮬레이션 결과 [ " << title << " ] ===\n";
    int t_wait[4]={}, t_turn[4]={}, count[4]={};
    int promoted_count = 0;
    for (const auto& p : processes) {
        int g = p.grade;
        t_wait[g] += p.waiting_time; t_turn[g] += p.turnaround_time; count[g]++;
        t_wait[0] += p.waiting_time; t_turn[0] += p.turnaround_time; count[0]++;
        if (p.is_promoted) promoted_count++;
    }
    cout << "\n[ 개별 프로세스 결과 ]\n";
    cout << "ID\tArrival\tStart\tComplete\tService\tTurnaround\tPromoted\n";
    cout << string(65,'-') << "\n";
    for (const auto& p : processes)
        cout << p.id << "\t" << p.arrival_time << "\t" << p.start_time << "\t"
             << p.completion_time << "\t" << p.service_time << "\t"
             << p.turnaround_time << "\t" << (p.is_promoted ? "Yes" : "No") << "\n";
    cout << fixed << setprecision(2);
    cout << "\n[ 등급별 평균 ATT ]\n";
    if(count[1]) cout << " - First   : ATT=" << (double)t_turn[1]/count[1] << " / Wait=" << (double)t_wait[1]/count[1] << " (" << count[1] << "명)\n";
    if(count[2]) cout << " - Business: ATT=" << (double)t_turn[2]/count[2] << " / Wait=" << (double)t_wait[2]/count[2] << " (" << count[2] << "명)\n";
    if(count[3]) cout << " - Economy : ATT=" << (double)t_turn[3]/count[3] << " / Wait=" << (double)t_wait[3]/count[3] << " (" << count[3] << "명)\n";
    cout << "\n[ 전체 통계 ]\n";
    cout << " - 전체 ATT: " << (double)t_turn[0]/count[0] << "\n";
    cout << " - 승격된 승객: " << promoted_count << "명\n";
}

int main() {
    vector<Process> processes = parseInputFile("input.txt");
    if (processes.empty()) return 1;

    BaselineA_FCFS baselineA;
    Scheduler schedulerA(processes, &baselineA);
    schedulerA.run(); printResults(processes, baselineA.getStrategyName()); resetProcesses(processes);

    BaselineB_Priority baselineB;
    Scheduler schedulerB(processes, &baselineB);
    schedulerB.run(); printResults(processes, baselineB.getStrategyName()); resetProcesses(processes);

    BaselineC_SJF baselineC;
    Scheduler schedulerC(processes, &baselineC);
    schedulerC.run(); printResults(processes, baselineC.getStrategyName()); resetProcesses(processes);

    ProposedStrategy proposed;
    Scheduler schedulerProp(processes, &proposed);
    schedulerProp.run();
    printResults(processes, proposed.getStrategyName());
    schedulerProp.printCounterStats();

    return 0;
}