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
        virtual T inc() = 0;
        virtual T load() const = 0;
        virtual void store(T n) = 0;
        virtual bool cond_store(T n, T& des) = 0;
};

template<typename T>
struct atomic_number : public number<T>
{
        virtual T atomic_inc() = 0;
        virtual T atomic_load() const = 0;
        virtual void atomic_store(T n) = 0;
        virtual bool compare_and_swap(T val, T& des) = 0;

        T inc() override
        {
                return atomic_inc();
        }

        T load() const override
        {
                return atomic_load();
        }

        void store(T n) override
        {
                atomic_store(n);
        }

        bool cond_store(T n, T& des) override
        {
                return compare_and_swap(n, des);
        }
};

template<typename T>
struct my_atomic_number
        : public atomic_number<T>
{
        explicit my_atomic_number(T i)
                : i_(i)
        {}

        T atomic_inc() final
        {
                return i_.fetch_add(1, std::memory_order_seq_cst);
        }

        T atomic_load() const final
        {
                return i_.load(std::memory_order_seq_cst);
        }

        void atomic_store(T n) final
        {
                i_.store(n, std::memory_order_seq_cst);
        }

        bool compare_and_swap(T val, T& des) final
        {
                return i_.compare_exchange_strong(des,
                                                  val,
                                                  std::memory_order_seq_cst);
        }

private:
        std::atomic<T> i_;
};

template<typename T>
struct abstract_number;

template<typename T>
struct abstract_number_creator_interface
{
        virtual
        std::shared_ptr<abstract_number<T>>
        create_new(T val) = 0;
};

template<typename T>
struct abstract_number
        : std::enable_shared_from_this<abstract_number<T>>
{
        abstract_number(std::shared_ptr<number<T>> n,
                        std::shared_ptr<
                                abstract_number_creator_interface<T>> creator)
                : n_(n), creator_(creator)
        {
        }

        std::shared_ptr<abstract_number<T>>
        operator++(int)
        {
                T cur = n_->load();
                T des = cur + 1;

                while(!n_->cond_store(des, cur))
                {
                        des = cur + 1;
                }

                auto prev = creator()->create_new(cur);

                return prev;
        }

        std::shared_ptr<abstract_number<T>>
        operator++()
        {
                T val = n_->inc();

                return this->shared_from_this();
        }

        std::shared_ptr<abstract_number<T>>
        operator=(std::shared_ptr<abstract_number<T>> other)
        {
                if(this->shared_from_this() == other)
                        return this->shared_from_this();

                T val = other->value();
                n_->store(val);

                return this->shared_from_this();

        }

        std::shared_ptr<abstract_number<T>>
        operator+=(std::shared_ptr<abstract_number<T>> b)
        {
                T cur = n_->load();
                T des = cur + b->value();

                while(!n_->cond_store(des, cur))
                {
                        des = cur + b->value();
                }

                return this->shared_from_this();
        }

        bool operator<=(std::shared_ptr<abstract_number<T>> b)
        {
                return this->value() <= b->value();
        }

        T value() const
        {
                return n_->load();
        }

        std::shared_ptr<abstract_number<T>>
        copy()
        {
                return creator()->create_new(value());
        }

        std::shared_ptr<abstract_number_creator_interface<T>> creator()
        {
                return creator_;
        }

private:
        std::shared_ptr<number<T>> n_;
        std::shared_ptr<abstract_number_creator_interface<T>> creator_;
};

template<typename T>
struct number_creator
{
        std::shared_ptr<number<T>> create_new(T val)
        {
                return std::make_shared<my_atomic_number<T>>(val);
        }
};

template<typename T>
struct shared_abstract_number_creator :
        abstract_number_creator_interface<T>,
        std::enable_shared_from_this<shared_abstract_number_creator<T>>
{

};

template<typename T>
struct abstract_number_creator :
        public shared_abstract_number_creator<T>

{
        abstract_number_creator()
                : ncreator_(std::make_shared<number_creator<T>>())
        {}

        std::shared_ptr<abstract_number<T>>
        create_new(T val) override
        {
                std::unique_lock<std::mutex> lock(mtx_);

                return std::make_shared<abstract_number<T>>(
                                ncreator_->create_new(val),
                                this->shared_from_this()
                );
        }

private:
        std::shared_ptr<number_creator<T>> ncreator_;
        std::mutex mtx_;
};

template<typename T>
std::ostream& operator<<(std::ostream& os, std::shared_ptr<abstract_number<T>> n)
{
        os << n->value();

        return os;
}

template<typename T, typename X>
void print_numbers(X n0, X n1, X s)
{
        static std::mutex mtx;

        std::unique_lock<std::mutex> lock(mtx);

        std::cout<<"n0="<<n0<<" n1="<<n1<<" s="<<s<<std::endl;
};


template<typename T>
static void sum(std::shared_ptr<abstract_number<T>> sum,
                       std::shared_ptr<abstract_number<T>> n0,
                       std::shared_ptr<abstract_number<T>> n1)
{
        auto number_creator = sum->creator();
        auto s = number_creator->create_new(0);

        s->operator=(n0);

        while(n0->operator<=(n1)) {

                print_numbers<T>(n0, n1, s);
                s->operator+=(n0->operator++(1));
        }


        sum->operator=(s);
}


template<typename T>
struct parallel_sum
{
        void operator()(std::shared_ptr<abstract_number<T>> sum,
                     std::shared_ptr<abstract_number<T>> n0,
                     std::shared_ptr<abstract_number<T>> n1)
        {
                std::vector<std::thread> pool;
                auto n = std::thread::hardware_concurrency();

                T slice = (n1->value() - n0->value()) / (T)n;
                for(decltype(n) i = 0; i < n; ++i)
                {

                        T l = slice * i;
                        T r = slice * (i + 1);

                        if(i + 1 == n)
                                r = n1->value();


                        auto my_elegant_modern_cpp11_lamda_function_for_extra_code_clarity =
                        [](std::shared_ptr<abstract_number<T>> sum,
                        std::shared_ptr<abstract_number<T>> n0,
                        std::shared_ptr<abstract_number<T>> n1)
                        {
                                auto number_creator = sum->creator();
                                auto s = number_creator->create_new(0);

                                s->operator=(n0);

                                while(n0->operator<=(n1)) {

                                        print_numbers<T>(n0, n1, s);
                                        s->operator+=(n0->operator++(1));
                                }


                                sum->operator=(s);
                        };

                        auto ln0 = n0->creator()->create_new(l);
                        auto rn1 = n1->creator()->create_new(r);

                        pool.push_back(std::thread(my_elegant_modern_cpp11_lamda_function_for_extra_code_clarity,
                                                   sum, ln0, rn1));
                }

                for(auto& t : pool)
                        t.join();


        }
};


template<typename T>
T do_shit()
{
        auto ncreator = std::make_shared<abstract_number_creator<T>>();

        auto sum = ncreator->create_new(0);
        auto n0  = ncreator->create_new(0);
        auto n1  = ncreator->create_new(100);

        parallel_sum<T>()(sum, n0, n1);

        return sum->value();
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