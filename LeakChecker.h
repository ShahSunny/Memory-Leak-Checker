#ifndef LEAKCHECKER_H
#define LEAKCHECKER_H

class LeakChecker {
public:
    LeakChecker();
    void generateLeakReport();
    ~LeakChecker();
private:
    static LeakChecker m_globalLeakChecker;
public:
    static LeakChecker* GetLeakChecker() {
        return &m_globalLeakChecker;
    }
};

#endif // LEAKCHECKER_H
