#include <stdlib.h>
#include <atomic>
#include <thread>
#include <iostream>
#include <mutex>
#include <vector>

#define UNUSED_PARAM(x) ((void)(x))


template<typename T>
struct number
{
        virtual T load() const = 0;
        virtual void store(T n) = 0;
        virtual T fetch_add(T n) = 0;
        virtual bool compare_and_swap(T val, T& expected) = 0;
};

template<typename T>
struct atomic_number : number<T>
{
        explicit atomic_number(T n)
                : n_(n)
        {}

        T load() const final
        {
                return n_.load();
        }

        void store(T n) final
        {
                n_.store(n);
        }

        bool compare_and_swap(T val, T& expected) final
        {
                return n_.compare_exchange_weak(expected,
                                                val);
        }

        T fetch_add(T n) final
        {
                return n_.fetch_add(n);
        }


private:
        std::atomic<T> n_;
};


template<typename T, typename U = atomic_number<T>>
struct abstract_number
{

        explicit abstract_number(T val) :
                n_(std::make_shared<typename std::decay<U>::type>(val)),
                cow_(false)
        {

        }

        abstract_number(const abstract_number& o) :
                n_(o.n_), cow_(true)
        {
        }

        abstract_number(abstract_number&& o) noexcept :
                n_(std::move(o.n_)), cow_(false)
        {

        }

        abstract_number&
        operator=(const abstract_number& o)
        {
                if(&o == this)
                        return *this;

                std::lock_guard<std::mutex> lock(cow_mtx_);
                n_ = o.n_;
                cow_ = true;

                return *this;
        }

        abstract_number&
        operator=(abstract_number&& o) noexcept
        {
                if(&o == this)
                        return *this;

                std::lock_guard<std::mutex> lock(cow_mtx_);
                n_ = std::move(o.n_);
                cow_ = false;

                return *this;
        }

        abstract_number
        operator++(int)
        {
                cow_check();

                abstract_number<T, U> tmp(*this);

                n_->fetch_add(1);

                return tmp;
        }

        abstract_number&
        operator++()
        {
                cow_check();

                n_->fetch_add(1);

                return *this;
        }


        abstract_number&
        operator+=(const abstract_number& b)
        {
                cow_check();

                // We don't know dependencies of b here
                // thus we can't be sure is changing b.val in between cas safe
                // Ideally DCAS needed or mutex on both objects

                auto expected = this->value();
                auto val = expected + b.value();

                while(!n_->compare_and_swap(val, expected))
                {
                        val = expected + b.value();
                }

                return *this;
        }

        bool operator<=(const abstract_number& b) const
        {
                return this->value() <= b.value();
        }

        T value() const
        {
                return n_->load();
        }

private:
        void cow_check()
        {
                if(!cow_)
                        return;

                std::lock_guard<std::mutex> lock(cow_mtx_);

                n_ = std::make_shared<typename std::decay<U>::type>(value());
                cow_ = false;
        }

private:
        std::shared_ptr<number<T>> n_;
        std::atomic_bool cow_;
        mutable std::mutex cow_mtx_;
};


template<typename T>
std::ostream& operator<<(std::ostream& os, const abstract_number<T>& n)
{
        os << n.value();

        return os;
}

static std::mutex stdout_mtx;

template<typename T>
void print_numbers(const abstract_number<T>& n0,
                   const abstract_number<T>& n1,
                   const abstract_number<T>& s)
{
        std::lock_guard<std::mutex> lock(stdout_mtx);

        std::cout<<"n0="<<n0<<" n1="<<n1<<" s="<<s<<std::endl;
};

//#define NO_MT

template<typename T>
struct parallel_sum
{
        void operator()(abstract_number<T> sum,
                        abstract_number<T> n0,
                        abstract_number<T> n1)
        {
                std::vector<std::thread> pool;
#if defined(NO_MT)
                auto n = 1;
#else
                auto n = std::thread::hardware_concurrency();
#endif

                T slice = (n1.value() - n0.value()) / (T)n;
                for(decltype(n) i = 0; i < n; ++i)
                {

                        T l = slice * i;
                        T r = slice * (i + 1);

                        if(i + 1 == n)
                                r = n1.value();


                        auto my_elegant_modern_cpp11_lamda_function_for_extra_code_clarity =
                        [](abstract_number<T> sum,
                        abstract_number<T> n0,
                        abstract_number<T> n1)
                        {
                                auto s = n0;

                                while(n0 <= n1) {

                                        print_numbers<T>(n0, n1, s);
                                        s += n0++;
                                }


                                sum = s;
                        };

                        abstract_number<T> ln0(l);
                        abstract_number<T> rn1(r);

#if defined(NO_MT)
                        my_elegant_modern_cpp11_lamda_function_for_extra_code_clarity(sum, ln0, rn1);
#else
                        pool.push_back(std::thread(my_elegant_modern_cpp11_lamda_function_for_extra_code_clarity,
                                                   sum, ln0, rn1));
#endif


                }

                for(auto& t : pool)
                        t.join();


        }
};


template<typename T>
T do_shit()
{
        abstract_number<T> sum (0);
        abstract_number<T> n0  (0);
        abstract_number<T> n1  (100);

        parallel_sum<T>()(sum, n0, n1);

        return sum.value();
}


int main(int argc, char** argv)
{
        UNUSED_PARAM(argc);
        UNUSED_PARAM(argv);

        auto shit = do_shit<uint64_t >();


        // RESULT CAN BE KIND OF UNEXPECTED BUT
        // THIS
        // IS
        // A
        // POWER
        // OF
        // C++11
        // FUCK YEAH!
        std::cout << "shit = " << shit << std::endl;


        return 0;
}