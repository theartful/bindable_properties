#include "bindable_properties.h"

#include <functional>
#include <memory>
#include <vector>

namespace bindable_properties
{
namespace details
{

struct binding_state {
    binding_state(const property_base& prop_) : prop{prop_} {}

    property_base prop;
    std::vector<property_base> deps;
};

static thread_local bool _is_currently_binding = false;
static thread_local std::shared_ptr<binding_state> _binding_state = {};

void set_currently_binding(bool val) { _is_currently_binding = val; }
bool is_currently_binding() { return _is_currently_binding; }

void set_current_prop(property_base* prop)
{
    _binding_state = std::make_shared<binding_state>(*prop);
}

void register_property(property_base* bound_prop)
{
    // don't register already registered props
    for (const auto& p : _binding_state->deps) {
        if (p.owner == bound_prop->owner)
            return;
    }

    _binding_state->deps.push_back(*bound_prop);

    std::shared_ptr<binding_state> state = _binding_state;

    _binding_state->deps.back().func = [state](property_base*, void*,
                                               details::call_type type) {
        if (type == details::call_type::notification) {
            if (state->prop.owner) {
                state->prop.owner->update();
            }
        }
    };
}

} // namespace details

property_base::property_base() noexcept :
    owner{this}, next{nullptr}, prev{nullptr}, func{}
{
}

property_base::property_base(const property_base& other) noexcept
{
    attach_to(other);
}

property_base::property_base(property_base&& other) noexcept : property_base()
{
    operator=(std::move(other));
}

property_base& property_base::operator=(const property_base& other)
{
    detach();
    attach_to(other);
    return *this;
}

property_base& property_base::operator=(property_base&& other)
{
    detach();

    attach_to(other);
    func = std::move(other.func);
    if (other.is_owner()) {
        property_base* crawler = &other;

        while (crawler != nullptr) {
            crawler->owner = this;
            crawler = crawler->next;
        }
    }
    other.detach();

    return *this;
}

property_base::~property_base() noexcept { detach(); }

void property_base::attach_to(const property_base& other)
{
    next = other.next;
    prev = const_cast<property_base*>(&other);
    if (other.next)
        other.next->prev = const_cast<property_base*>(this);
    other.next = const_cast<property_base*>(this);
    owner = other.owner;
}

void property_base::detach()
{
    if (is_owner()) {
        if (next) {
            property_base* crawler = next;
            crawler->prev = nullptr;

            while (crawler != nullptr) {
                crawler->owner = nullptr;
                crawler = crawler->next;
            }
        }
    } else {
        if (prev)
            prev->next = next;
        if (next)
            next->prev = prev;
    }

    next = nullptr;
    prev = nullptr;
    owner = nullptr;
}

void property_base::notify_all(void* value)
{
    // we first change the values of all copies, then we notify them
    // because one of the notifications may use the value of other
    // copy of the same property
    property_base* crawler = this;
    while (crawler != nullptr) {
        if (crawler->func)
            crawler->func(crawler, value,
                          details::call_type::initial_notification);
        crawler = crawler->next;
    }

    crawler = this;
    while (crawler != nullptr) {
        if (crawler->func)
            crawler->func(crawler, value, details::call_type::notification);
        crawler = crawler->next;
    }
}

void property_base::update()
{
    if (func) {
        func(this, nullptr, details::call_type::binding);
    }
}

int property_base::num_views() const
{
    if (is_zombie())
        return 0;

    int result = 0;
    property_base* crawler = owner->next;
    while (crawler != nullptr) {
        result++;
        crawler = crawler->next;
    }

    return result;
}

} // namespace bindable_properties
