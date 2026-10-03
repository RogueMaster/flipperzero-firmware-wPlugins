/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The PIVLink card session class for piv_command.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_COMMAND_HPP_
#define TINY_CRYPTO_PIV_COMMAND_HPP_

#ifndef __cplusplus
#error Do not include piv_command.hpp in a C project, include piv_command.h instead
#endif

#include <tiny_crypto/apdu.hpp>
#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND

namespace tiny_crypto {

typedef ::TC_PIV_result piv_result;
typedef ::TC_PIV_interface piv_interface;
typedef ::TC_PIV_application_id piv_application_id;
typedef ::TC_PIV_command piv_command;
typedef ::TC_PIV_status piv_status;
typedef ::TC_PIV_link_options piv_link_options;
typedef ::TC_PIV_link_info piv_link_info;
typedef ::TC_PIV_application piv_application;
typedef ::TC_PIV_data_object piv_data_object;
typedef ::TC_PIV_reference_status piv_reference_status;

/* TC_PIV_application_read. out borrows response. */
TC_CPP_NODISCARD inline TC_TLV_result piv_application_read(bytes response,
                                                           piv_application_id expected,
                                                           unsigned flags,
                                                           piv_application& out) noexcept
{
  return ::TC_PIV_application_read(response, expected, flags, &out);
}

/* TC_PIV_status_classify. */
TC_CPP_NODISCARD inline piv_status piv_status_classify(uint16_t sw, piv_command command,
                                                       piv_application_id application,
                                                       unsigned* retries = nullptr) noexcept
{
  return ::TC_PIV_status_classify(sw, command, application, retries);
}

// A card session. The destructor calls TC_PIV_link_clear, which wipes the
// borrowed command scratch, so the scratch and the transport context must
// outlive the object. Copying or moving would duplicate the channel state.
class PIVLink {
  ::TC_PIV_link link_;

public:
  PIVLink() noexcept : link_{}
  {}
  ~PIVLink() noexcept
  {
    clear();
  }
  PIVLink(const PIVLink&) = delete;
  PIVLink& operator=(const PIVLink&) = delete;
  PIVLink(PIVLink&&) = delete;
  PIVLink& operator=(PIVLink&&) = delete;

  TC_CPP_NODISCARD piv_result init(apdu_transport transport, const piv_link_options& options,
                                   buffer command_scratch) noexcept
  {
    return ::TC_PIV_link_init(&link_, transport, &options, command_scratch);
  }
  void clear() noexcept
  {
    ::TC_PIV_link_clear(&link_);
  }
  TC_CPP_NODISCARD piv_result select(piv_application_id application, unsigned flags,
                                     buffer response, piv_application& out) noexcept
  {
    return ::TC_PIV_select(&link_, application, flags, response, &out);
  }
  TC_CPP_NODISCARD piv_result get_data(bytes tag, buffer response, piv_data_object& out) noexcept
  {
    return ::TC_PIV_get_data(&link_, tag, response, &out);
  }
  TC_CPP_NODISCARD piv_result verify_status(uint8_t reference, piv_reference_status& out) noexcept
  {
    return ::TC_PIV_verify_status(&link_, reference, &out);
  }
  TC_CPP_NODISCARD piv_result pin_verify(uint8_t reference, bytes pin, unsigned minimum_retries,
                                         piv_reference_status& out) noexcept
  {
    return ::TC_PIV_pin_verify(&link_, reference, pin, minimum_retries, &out);
  }
  TC_CPP_NODISCARD uint16_t status() const noexcept
  {
    return ::TC_PIV_link_status(&link_);
  }
  TC_CPP_NODISCARD piv_link_info info() const noexcept
  {
    piv_link_info out{};
    ::TC_PIV_link_info_get(&link_, &out);
    return out;
  }
  // The C link, for the layers that take a TC_PIV_link pointer.
  TC_CPP_NODISCARD ::TC_PIV_link* native() noexcept
  {
    return &link_;
  }
};

} // namespace tiny_crypto
#endif
#endif
