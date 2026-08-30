#ifndef NETWORKING_NAT_TEST_SUPPORT_HPP
#define NETWORKING_NAT_TEST_SUPPORT_HPP

#include "../../Modules/Networking/networking_nat_traversal.hpp"

enum class networking_nat_model_type : uint8_t
{
    FULL_CONE = 0U,
    ADDRESS_RESTRICTED = 1U,
    PORT_RESTRICTED = 2U,
    SYMMETRIC = 3U,
    DOUBLE_NAT = 4U,
    UDP_BLOCKED = 5U
};

class networking_nat_test_model : public networking_nat_probe_io
{
    private:
        networking_nat_model_type _type;
        ft_bool _mapped;
        networking_message_endpoint _last_remote;

        static ft_bool same_host(const networking_message_endpoint &left,
            const networking_message_endpoint &right) noexcept
        {
            if (left.length == 0U || right.length == 0U
                || left.address.ss_family != right.address.ss_family)
                return (FT_FALSE);
            if (left.address.ss_family == AF_INET)
            {
                const sockaddr_in *left_address;
                const sockaddr_in *right_address;

                left_address = reinterpret_cast<const sockaddr_in *>(
                    &left.address);
                right_address = reinterpret_cast<const sockaddr_in *>(
                    &right.address);
                if (ft_memcmp(&left_address->sin_addr,
                    &right_address->sin_addr, sizeof(left_address->sin_addr))
                    == 0)
                    return (FT_TRUE);
                return (FT_FALSE);
            }
            if (left.address.ss_family == AF_INET6)
            {
                const sockaddr_in6 *left_address;
                const sockaddr_in6 *right_address;

                left_address = reinterpret_cast<const sockaddr_in6 *>(
                    &left.address);
                right_address = reinterpret_cast<const sockaddr_in6 *>(
                    &right.address);
                if (ft_memcmp(&left_address->sin6_addr,
                    &right_address->sin6_addr,
                    sizeof(left_address->sin6_addr)) == 0)
                    return (FT_TRUE);
                return (FT_FALSE);
            }
            return (FT_FALSE);
        }

    public:
        networking_nat_test_model() noexcept
            : networking_nat_probe_io(), _type(networking_nat_model_type::FULL_CONE),
              _mapped(FT_FALSE), _last_remote()
        {
            ft_memset(&this->_last_remote, 0, sizeof(this->_last_remote));
            return ;
        }

        ~networking_nat_test_model() noexcept override
        {
            return ;
        }

        int32_t initialize(networking_nat_model_type type) noexcept
        {
            this->_type = type;
            this->_mapped = FT_FALSE;
            ft_memset(&this->_last_remote, 0, sizeof(this->_last_remote));
            return (FT_ERR_SUCCESS);
        }

        int32_t send_probe(const networking_message_endpoint &local,
            const networking_message_endpoint &remote, uint64_t attempt_id)
            noexcept override
        {
            (void)local;
            (void)attempt_id;
            if (remote.length == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (this->_type == networking_nat_model_type::UDP_BLOCKED)
                return (FT_ERR_SOCKET_SEND_FAILED);
            this->_last_remote = remote;
            this->_mapped = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }

        ft_bool accepts_response(
            const networking_message_endpoint &remote) const noexcept
        {
            if (this->_mapped == FT_FALSE || remote.length == 0U)
                return (FT_FALSE);
            if (this->_last_remote.address.ss_family
                != remote.address.ss_family)
                return (FT_FALSE);
            if (this->_type == networking_nat_model_type::FULL_CONE)
                return (FT_TRUE);
            if (this->_type == networking_nat_model_type::DOUBLE_NAT
                || this->_type == networking_nat_model_type::UDP_BLOCKED)
                return (FT_FALSE);
            if (this->_type == networking_nat_model_type::ADDRESS_RESTRICTED)
                return (networking_nat_test_model::same_host(
                    this->_last_remote, remote));
            if (this->_last_remote.length == remote.length
                && ft_memcmp(&this->_last_remote.address, &remote.address,
                    remote.length) == 0)
                return (FT_TRUE);
            return (FT_FALSE);
        }
};

#endif
