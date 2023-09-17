
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <condition_variable>

#include "defines.h"
#include "variables.h"

#include <cannetwork.hpp>
#include <eposnetwork.hpp>
#include <eposnode.hpp>

using namespace std;

CanNetwork can(r "can0");

timer_t timerId;

long time_k = 0;
long time_total = 0;
volatile float time_s;

condition_variable timer0;
condition_variable timerCan;
atomic<bool> END(false);

vector<long> pos3_vec;
vector<long> pos4_vec;

void timer_isr(int sig, siginfo_t *p, void *p2)
{
    time_total++;
    time_s = time_total * _ts_s;

    if (time_s >= T_total)
        END = true;

    // timerCan.notify_all();

    if ((time_total % _count_k) == 0)
    {
        time_k++;
        timer0.notify_all();
    }
}

int createTimer()
{
    struct sigevent sev;
    struct itimerspec its;

    std::memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;

    if (timer_create(CLOCK_REALTIME, &sev, &timerId) == -1)
    {
        perror("timer_create");
        return EXIT_FAILURE;
    }

    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = _ts_n;

    if (timer_settime(timerId, 0, &its, NULL) == -1)
    {
        perror("timer_settime");
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_isr;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1)
    {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    return 0;
}

void *_threadSendCan(void *arg)
{
    auto us0 = std::chrono::high_resolution_clock::now();
    auto us1 = std::chrono::high_resolution_clock::now() - us0;
    long long microseconds;

    auto duracion = std::chrono::duration_cast<std::chrono::microseconds>(us1);

    mutex mtx;
    long t_k = 0;
    while (END == false)
    {

        us0 = std::chrono::high_resolution_clock::now();

        can.writeAsync();

        can.fillDic();
        EXO::net.readPDO_TX_all();

        t_k = 0;
        do
        {
            us1 = std::chrono::high_resolution_clock::now() - us0;
            duracion = std::chrono::duration_cast<std::chrono::microseconds>(us1);
            t_k++;
        } while (duracion.count() < _ts_u);

        if (t_k == 1)
        {
            printf("[_threadSendCan] timeout %lld\n", duracion.count());
        }
    }

    printf("[END] _threadSendCan \n");
    return nullptr;
}

void *_threadSync(void *arg)
{
    long t_k = 0;
    mutex mtx;
    long can_sz = can._queued_frames.size();

    while (END == false)
    {
        {
            std::unique_lock<std::mutex> lock(mtx);
            timer0.wait(lock);
        }

        t_k = time_k;
        can_sz = can._queued_frames.size();

        if (t_k % 200 == 0)
        {
            printf("[%3.4f][_threadSync] SZ_CAN: %2ld  pos_3:%10ld pos_4: %10ld \n",
                   time_s,
                   can_sz,
                   EXO::R::Knee::Encoder.getPosition(),
                   EXO::R::Knee::Motor.getPosition());
        }
    }
    return nullptr;
}

void *_threadControl(void *arg)
{

    long i = 0;
    long t_k = 0;

    EXO::R::Knee::Motor.startMotors();
    EXO::R::Knee::Encoder.startMotors();

    EXO::R::Knee::Encoder.stopMotors();

    EXO::R::Knee::Motor.setVelocityMode();

    mutex mtx;
    auto us0 = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    auto us1 = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    while (END == false)
    {
        EXO::net.sync(true);

        {
            std::unique_lock<std::mutex> lock(mtx);
            timer0.wait(lock);
        }

        us0 = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());

        if ((t_k + 1) != time_k)
        {
            printf("Error, este bucle est'a tardando demasiado %ld\n", t_k);
        }
        t_k = time_k;

        static bool step1 = false;

        if (time_s < 40.0 && !step1)
        {
            i++;

            if (abs(EXO::R::Knee::Encoder.getPosition()) > 100)
            {
                EXO::R::Knee::Motor.setVelocity(EXO::R::Knee::Encoder.getPosition() * 10);
            }
            else
            {
                EXO::R::Knee::Motor.setVelocity(0);
            }
        }

        if (time_s >= 40.0 && !step1)
        {
            EXO::R::Knee::Motor.stopMotors();
            step1 = true;
        }

        us1 = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());

        pos3_vec[t_k] = EXO::R::Knee::Encoder.getPosition();
        pos4_vec[t_k] = EXO::R::Knee::Motor.getPosition();

        // printf("pos: %d\n", EXO::R::Knee::Encoder.getPosition());
        // printf("vel: %d\n", EXO::R::Knee::Encoder.getVelocity());
        // printf("us: %lld \n\n", us1 - us0);
    }
    EXO::R::Knee::Motor.stopMotors();
    EXO::R::Knee::Encoder.stopMotors();

    return nullptr;
}

// void my_handler(int s)
// {
//     printf("Caught signal %d\n", s);
//     exit(1);
// }

int main()
{

    // struct sigaction sigIntHandler;

    // sigIntHandler.sa_handler = my_handler;
    // sigemptyset(&sigIntHandler.sa_mask);
    // sigIntHandler.sa_flags = 0;

    // sigaction(SIGINT, &sigIntHandler, NULL);

    if (createTimer() != 0)
    {
        return EXIT_FAILURE;
    }

    pos3_vec.reserve(T_total * 1000 + 2 * 1000);
    pos4_vec.reserve(T_total * 1000 + 2 * 1000);

    can.connect();

    EXO::net.setNode(3, EXO::R::Knee::Encoder);
    EXO::net.setNode(4, EXO::R::Knee::Motor);

    EXO::net.init();
    EXO::net.sync(false);

    EXO::R::Knee::Motor.stopMotors();
    EXO::R::Knee::Encoder.stopMotors();

    EXO::R::Knee::Motor.startMotors();
    EXO::R::Knee::Encoder.startMotors();

    EXO::net.sync(false);

    pthread_t threadSync, threadControl, threadSendCan;
    
    pthread_attr_t attr0;
    pthread_attr_init(&attr0);
    struct sched_param params0;
    params0.sched_priority = 99;
    pthread_attr_setschedpolicy(&attr0, SCHED_FIFO);
    pthread_attr_setschedparam(&attr0, &params0);

    pthread_attr_t attr1;
    pthread_attr_init(&attr1);
    struct sched_param params1;
    params1.sched_priority = 10;
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    pthread_attr_setschedparam(&attr1, &params1);

    pthread_attr_t attr2;
    pthread_attr_init(&attr2);
    struct sched_param params2;
    params2.sched_priority = 70;
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    pthread_attr_setschedparam(&attr2, &params2);

    

    if (pthread_create(&threadSendCan, &attr0, _threadSendCan, nullptr) != 0)
    {
        std::cerr << "Error al crear el hilo." << std::endl;
        return 1;
    }

    if (pthread_create(&threadSync, &attr1, _threadSync, nullptr) != 0)
    {
        std::cerr << "Error al crear el hilo." << std::endl;
        return 1;
    }

    if (pthread_create(&threadControl, &attr2, _threadControl, nullptr) != 0)
    {
        std::cerr << "Error al crear el hilo." << std::endl;
        return 1;
    }

    pthread_attr_destroy(&attr0);
    pthread_attr_destroy(&attr1);
    pthread_attr_destroy(&attr2);


    // Wait for n seconds

    if (pthread_join(threadSync, nullptr) != 0)
    {
        std::cerr << "Error al esperar al hilo." << std::endl;
        return 1;
    }
    if (pthread_join(threadControl, nullptr) != 0)
    {
        std::cerr << "Error al esperar al hilo." << std::endl;
        return 1;
    }
    if (pthread_join(threadSendCan, nullptr) != 0)
    {
        std::cerr << "Error al esperar al hilo." << std::endl;
        return 1;
    }

    can.disconnect();

    timer_delete(timerId);

    // scp debian@192.168.6.2:/home/debian/build/pos_vec.dat .
    FILE *f1;

    f1 = fopen("pos_vec.dat", "w+");

    if (f1 != nullptr)
    {
        for (int i = 0; i < 10 * 1000; i++)
        {
            fprintf(f1, "%10ld\t%10ld\n", pos3_vec[i], pos4_vec[i]);
        }

        fclose(f1);
    }
    else
    {
        printf("Error guardando el archivo");
    }

    return EXIT_SUCCESS;
}
