/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* The PIV and TWIC catalogs and the PIVInventory class for piv_catalog.h.
 * Contracts, statuses and lifetimes follow the C header. Conventions:
 * docs/cpp.md. Library-wide contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_CATALOG_HPP_
#define TINY_CRYPTO_PIV_CATALOG_HPP_

#ifndef __cplusplus
#error Do not include piv_catalog.hpp in a C project, include piv_catalog.h instead
#endif

#include <tiny_crypto/common.hpp>
#include <tiny_crypto/piv_catalog.h>
#include <tiny_crypto/piv_command.hpp>
#if TC_ENABLE_PIV_CATALOG

namespace tiny_crypto {

typedef ::TC_PIV_access piv_access;
typedef ::TC_PIV_requirement piv_requirement;
typedef ::TC_PIV_object_kind piv_object_kind;
typedef ::TC_PIV_object_state piv_object_state;
typedef ::TC_PIV_object_info piv_object_info;
typedef ::TC_PIV_object piv_object;
typedef ::TC_PIV_inventory_plan piv_inventory_plan;

/* TC_PIV_catalog_count. */
TC_CPP_NODISCARD inline size_t piv_catalog_count(piv_application_id application,
                                                 TC_PIV_card_profile profile) noexcept
{
  return ::TC_PIV_catalog_count(application, profile);
}

/* TC_PIV_catalog_at. The entry lives for the program. */
TC_CPP_NODISCARD inline const piv_object_info*
piv_catalog_at(piv_application_id application, TC_PIV_card_profile profile, size_t index) noexcept
{
  return ::TC_PIV_catalog_at(application, profile, index);
}

/* TC_PIV_catalog_find. */
TC_CPP_NODISCARD inline const piv_object_info*
piv_catalog_find(piv_application_id application, TC_PIV_card_profile profile, bytes tag) noexcept
{
  return ::TC_PIV_catalog_find(application, profile, tag);
}

// An inventory over a caller object array. The destructor calls
// TC_PIV_inventory_clear, which wipes the pool bytes the objects use, so the
// object array and the pool must outlive the object. Copying or moving would
// leave two owners of one pool.
class PIVInventory {
  ::TC_PIV_inventory inventory_;

public:
  PIVInventory(piv_object* objects, size_t capacity) noexcept : inventory_{}
  {
    inventory_.objects = objects;
    inventory_.capacity = capacity;
  }
  template <size_t N>
  explicit PIVInventory(piv_object (&objects)[N]) noexcept : PIVInventory(objects, N)
  {}
  ~PIVInventory() noexcept
  {
    clear();
  }
  PIVInventory(const PIVInventory&) = delete;
  PIVInventory& operator=(const PIVInventory&) = delete;
  PIVInventory(PIVInventory&&) = delete;
  PIVInventory& operator=(PIVInventory&&) = delete;

  /* TC_PIV_inventory_read. plan may be nullptr. */
  TC_CPP_NODISCARD piv_result read(PIVLink& link, const piv_inventory_plan* plan, buffer pool,
                                   size_t& work) noexcept
  {
    return ::TC_PIV_inventory_read(link.native(), plan, pool, &work, &inventory_);
  }
  TC_CPP_NODISCARD const piv_object* find(uint16_t container) const noexcept
  {
    return ::TC_PIV_inventory_find(&inventory_, container);
  }
  TC_CPP_NODISCARD size_t size() const noexcept
  {
    return inventory_.count;
  }
  TC_CPP_NODISCARD const piv_object& operator[](size_t index) const noexcept
  {
    return inventory_.objects[index];
  }
  TC_CPP_NODISCARD size_t pool_used() const noexcept
  {
    return inventory_.pool_used;
  }
  void clear() noexcept
  {
    ::TC_PIV_inventory_clear(&inventory_);
  }
  // The C inventory, for the layers that take a TC_PIV_inventory pointer.
  TC_CPP_NODISCARD const ::TC_PIV_inventory* native() const noexcept
  {
    return &inventory_;
  }
};

} // namespace tiny_crypto
#endif
#endif
