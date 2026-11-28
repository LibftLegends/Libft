#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_istream.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

class test_cpp_class_istream_probe : public ft_istream
{
    public:
        char        _storage[16];
        ft_size_t   _length;
        ft_size_t   _position;

        test_cpp_class_istream_probe() noexcept
            : ft_istream(), _storage(), _length(0), _position(0)
        {
            return ;
        }

        test_cpp_class_istream_probe(
            const test_cpp_class_istream_probe &other) noexcept = delete;

        test_cpp_class_istream_probe(
            test_cpp_class_istream_probe &&other) noexcept = delete;

        ~test_cpp_class_istream_probe() noexcept
        {
            (void)this->destroy();
            return ;
        }

        int32_t initialize(const char *value) noexcept
        {
            ft_size_t index;

            FT_ASSERT(value != ft_nullptr);
            if (ft_istream::initialize() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index = 0;
            while (index < 15 && value[index] != '\0')
            {
                this->_storage[index] = value[index];
                index++;
            }
            this->_storage[index] = '\0';
            this->_length = index;
            this->_position = 0;
            return (FT_ERR_SUCCESS);
        }

        int32_t initialize(const test_cpp_class_istream_probe &other) noexcept
        {
            ft_size_t index;
            int32_t initialize_error;

            initialize_error = ft_istream::initialize();
            if (initialize_error != FT_ERR_SUCCESS)
                return (initialize_error);
            index = 0;
            while (index < 16)
            {
                this->_storage[index] = other._storage[index];
                index++;
            }
            this->_length = other._length;
            this->_position = other._position;
            this->_gcount = other._gcount;
            this->_is_valid = other._is_valid;
            if (other._mutex != ft_nullptr)
            {
                initialize_error = this->enable_thread_safety();
                if (initialize_error != FT_ERR_SUCCESS)
                {
                    (void)this->destroy();
                    return (initialize_error);
                }
            }
            return (FT_ERR_SUCCESS);
        }

        int32_t move(test_cpp_class_istream_probe &other) noexcept
        {
            ft_size_t index;
            int32_t move_error;

            if (&other == this)
                return (FT_ERR_SUCCESS);
            move_error = ft_istream::move(other);
            if (move_error != FT_ERR_SUCCESS)
                return (move_error);
            index = 0;
            while (index < 16)
            {
                this->_storage[index] = other._storage[index];
                other._storage[index] = '\0';
                index++;
            }
            this->_length = other._length;
            this->_position = other._position;
            other._length = 0;
            other._position = 0;
            return (FT_ERR_SUCCESS);
        }

        int32_t destroy() noexcept
        {
            this->_length = 0;
            this->_position = 0;
            return (ft_istream::destroy());
        }

    protected:
        ssize_t do_read(char *buffer, ft_size_t count)
        {
            ft_size_t copied_count;

            copied_count = 0;
            while (copied_count < count && this->_position < this->_length)
            {
                buffer[copied_count] = this->_storage[this->_position];
                copied_count++;
                this->_position++;
            }
            return (static_cast<ssize_t>(copied_count));
        }
};

FT_TEST(test_cpp_class_istream_copy_constructor_preserves_read_state)
{
    test_cpp_class_istream_probe source_stream;
    char buffer[2];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize("abc"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.enable_thread_safety());
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(source_stream.read(buffer, 1)));
    FT_ASSERT_EQ('a', buffer[0]);

    test_cpp_class_istream_probe copied_stream;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.initialize(source_stream));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, copied_stream._initialised_state);
    FT_ASSERT_EQ(1U, copied_stream.gcount());
    FT_ASSERT_EQ(FT_TRUE, copied_stream.is_valid());
    FT_ASSERT_EQ(FT_TRUE, copied_stream.is_thread_safe());
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(copied_stream.read(buffer, 1)));
    FT_ASSERT_EQ('b', buffer[0]);
    FT_ASSERT_EQ(1U, copied_stream.gcount());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_stream.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istream_move_constructor_preserves_read_state)
{
    test_cpp_class_istream_probe source_stream;
    char buffer[2];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.initialize("xyz"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stream.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source_stream._mutex;
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(source_stream.read(buffer, 1)));
    FT_ASSERT_EQ('x', buffer[0]);

    test_cpp_class_istream_probe moved_stream;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.move(source_stream));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_stream._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, source_stream._mutex);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, moved_stream._initialised_state);
    FT_ASSERT_EQ(1U, moved_stream.gcount());
    FT_ASSERT_EQ(FT_TRUE, moved_stream.is_valid());
    FT_ASSERT_EQ(FT_TRUE, moved_stream.is_thread_safe());
    FT_ASSERT_EQ(source_mutex, moved_stream._mutex);
    buffer[0] = '\0';
    buffer[1] = '\0';
    FT_ASSERT_EQ(1, static_cast<int>(moved_stream.read(buffer, 1)));
    FT_ASSERT_EQ('y', buffer[0]);
    FT_ASSERT_EQ(1U, moved_stream.gcount());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_stream.destroy());
    return (1);
}

FT_TEST(test_cpp_class_istream_read_preserves_state_when_lock_fails)
{
    test_cpp_class_istream_probe stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize("abc"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.enable_thread_safety());
    char byte = '\0';
    FT_ASSERT_EQ(1, stream.read(&byte, 1));
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED);
    const ssize_t result = stream.read(&byte, 1);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, stream.get_error());
    FT_ASSERT_EQ('a', byte);
    FT_ASSERT_EQ(1U, stream.gcount());
    FT_ASSERT_EQ(FT_TRUE, stream.is_valid());
    FT_ASSERT_EQ(-1, stream.read(ft_nullptr, 1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.get_error());
    FT_ASSERT_EQ(0U, stream.gcount());
    FT_ASSERT_EQ(FT_FALSE, stream.is_valid());
    FT_ASSERT_EQ(1, stream.read(&byte, 1));
    FT_ASSERT_EQ('b', byte);
    return (1);
}
