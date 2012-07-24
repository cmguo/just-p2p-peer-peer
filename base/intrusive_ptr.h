#ifndef INTRUSIVE_PTR_H
#define INTRUSIVE_PTR_H

namespace base
{
    template<class T>
    class intrusive_ptr_base
    {
    public:
        intrusive_ptr_base()
            : reference_count_(0)
        {

        }

        virtual ~intrusive_ptr_base()
        {

        }

        friend void intrusive_ptr_add_ref(T * s)
        {
            assert(s->reference_count_ >= 0);
            assert(s != 0);
            ++s->reference_count_;
        }

        friend void intrusive_ptr_release(T * s)
        {
            assert(s->reference_count_ > 0);
            assert(s != 0);
            if (--s->reference_count_ == 0)
            {
                delete s;
                s = NULL;
            }
        }

    private:
        boost::uint32_t reference_count_;
    };
}

#endif