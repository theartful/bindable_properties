#ifndef BINDABLE_PROPERTIES_H
#define BINDABLE_PROPERTIES_H

#include <functional>
#include <type_traits>
#include <vector>

namespace bindable_properties
{

class property_base;

template <typename T>
class property;

namespace details
{
#ifdef __cpp_lib_is_invocable
template <class T, class... Args>
using is_invocable = std::is_invocable<T, Args...>;
template <class F, class... Args>
using invoke_result = std::invoke_result<F, Args...>;
#else
// https://stackoverflow.com/questions/51187974/can-stdis-invocable-be-emulated-within-c11
template <typename F, typename... Args>
struct is_invocable
    : std::is_constructible<
          std::function<void(Args...)>,
          std::reference_wrapper<typename std::remove_reference<F>::type>> {
};

template <typename R, typename F, typename... Args>
struct is_invocable_r
    : std::is_constructible<
          std::function<R(Args...)>,
          std::reference_wrapper<typename std::remove_reference<F>::type>> {
};

template <class F, class... Args>
using invoke_result = std::result_of<F(Args...)>;
#endif

bool is_currently_binding();
void set_currently_binding(bool);
void register_property(property_base*);
void set_current_prop(property_base*);

enum class call_type {
    initial_binding,
    binding,
    setter,
    initial_notification,
    notification
};

template <typename T>
struct default_setter {
    void operator()(property_base* prop, void* value, call_type)
    {
        property<T>* prop_casted = static_cast<property<T>*>(prop);
        T* value_casted = static_cast<T*>(value);

        *prop_casted = *value_casted;
    }
};

template <typename T>
struct default_notifier {
    void operator()(property_base* prop, void* value, call_type)
    {
        property<T>* prop_casted = static_cast<property<T>*>(prop);
        T* value_casted = static_cast<T*>(value);

        prop_casted->val = *value_casted;
    }
};

struct nop {
    template <typename... Args>
    void operator()(Args&&...)
    {
    }
};

template <typename Lambda>
struct arguments_adapter {

    // using reference wrapper to prevent implicit conversion from
    // property<T>& to T
    template <typename T>
    using property_ref = std::reference_wrapper<property<T>>;

    template <typename T>
    void operator()(property<T>& prop, const T& value)
    {
        return apply(prop, value);
    }

    template <typename T>
    void operator()(property<T>& prop)
    {
        return lambda(prop);
    }

    template <typename T>
    void operator()(const T& value)
    {
        return lambda(value);
    }

    template <typename T>
    typename std::enable_if<
        details::is_invocable<Lambda, property_ref<T>, const T&>::value>::type
    apply(property<T>& prop, const T& value)
    {
        lambda(prop, value);
    }

    template <typename T>
    typename std::enable_if<
        details::is_invocable<Lambda, property_ref<T>>::value &&
        !details::is_invocable<Lambda, property_ref<T>, const T&>::value>::type
    apply(property<T>& prop, const T& /* value */)
    {
        lambda(prop);
    }

    template <typename T>
    typename std::enable_if<
        details::is_invocable<Lambda, const T&>::value &&
        !details::is_invocable<Lambda, property_ref<T>>::value &&
        !details::is_invocable<Lambda, property_ref<T>, const T&>::value>::type
    apply(property<T>& /* prop */, const T& value)
    {
        lambda(value);
    }

    template <typename T>
    typename std::enable_if<
        details::is_invocable<Lambda>::value &&
        !details::is_invocable<Lambda, const T&>::value &&
        !details::is_invocable<Lambda, property_ref<T>>::value &&
        !details::is_invocable<Lambda, property_ref<T>, const T&>::value>::type
    apply(property<T>& /* prop */, const T& /* value */)
    {
        lambda();
    }

    Lambda lambda;
};

template <typename T, typename BindingLambda, typename SetterLambda,
          typename NotifierLambda>
struct property_binder {
    property_binder(BindingLambda binding_, SetterLambda setter_,
                    NotifierLambda notifier_) :
        binding{binding_}, setter{setter_}, notifier{notifier_}
    {
    }

    void operator()(property_base* prop, void* value, call_type type)
    {
        property<T>* prop_casted = static_cast<property<T>*>(prop);

        switch (type) {
        case call_type::initial_binding:
            set_currently_binding(true);
            set_current_prop(prop);
            prop_casted->set_directly_as_owner(binding());
            set_currently_binding(false);
            break;
        case call_type::binding:
            prop_casted->set_directly_as_owner(binding());
            break;
        case call_type::setter: {
            setter(*prop_casted, *static_cast<T*>(value));
            break;
        }
        case call_type::notification:
            notifier(*prop_casted, prop_casted->value());
            break;
        default:
            // this should never happen
            break;
        }
    }

    BindingLambda binding;
    arguments_adapter<SetterLambda> setter;
    arguments_adapter<NotifierLambda> notifier;
};

template <typename T, typename NotifierLambda>
struct property_notifier {
    property_notifier(NotifierLambda notifier_) : notifier{notifier_} {}

    void operator()(property_base* prop, void* value, call_type type)
    {
        property<T>* prop_casted = static_cast<property<T>*>(prop);
        T* value_casted = static_cast<T*>(value);

        switch (type) {
        case call_type::initial_notification:
            prop_casted->val = *value_casted;
            break;
        case call_type::notification:
            notifier(*prop_casted, *value_casted);
            break;
        case call_type::setter:
            if (prop_casted->is_owner())
                prop_casted->set_directly_as_owner(*value_casted);
            break;
        default:
            // this should never happen
            break;
        }
    }

    arguments_adapter<NotifierLambda> notifier;
};

template <typename T, typename SetterLambda>
struct property_setter {
    property_setter(SetterLambda setter_) : setter{setter_} {}

    void operator()(property_base* prop, void* value, call_type type)
    {
        property<T>* prop_casted = static_cast<property<T>*>(prop);
        T* value_casted = static_cast<T*>(value);

        switch (type) {
        case call_type::setter:
            setter(*prop_casted, *value_casted);
            break;
        default:
            break;
        }
    }

    arguments_adapter<SetterLambda> setter;
};

} // namespace details

class property_base
{
    template <typename T>
    friend class property;
    friend void details::register_property(property_base*);

public:
    property_base() noexcept;
    property_base(const property_base& other) noexcept;
    property_base(property_base&& other) noexcept;
    ~property_base() noexcept;

    property_base& operator=(const property_base& other);
    property_base& operator=(property_base&& other);
    bool is_owner() const { return this == owner; }
    bool is_zombie() const { return owner == nullptr; }
    bool is_view() const { return !is_owner() && !is_zombie(); }

    int num_views() const;

protected:
    void attach_to(const property_base& other);
    void detach();
    void notify_all(void* value);
    void update();

protected:
    property_base* owner;
    mutable property_base* next;
    mutable property_base* prev;

    std::function<void(property_base*, void*, details::call_type)> func;
};

template <typename T>
class property : public property_base
{
    using self = property<T>;

    template <typename U, typename NotifierLambda>
    friend class details::property_notifier;

    template <typename U, typename NotifierLambda>
    friend class details::property_setter;

    template <typename U, typename BindingLambda, typename SetterLambda,
              typename NotifierLambda>
    friend class details::property_binder;

    template <typename U>
    friend class details::default_notifier;

public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;

    property(const value_type& initial = {}) noexcept :
        property_base{}, val{initial}
    {
        func = details::default_setter<T>{};
    }

    property(self&& other) noexcept : property_base(std::move(other))
    {
        val = other.val;

        if (is_owner()) {
            func = details::default_setter<T>{};
        } else {
            func = details::default_notifier<T>{};
        }
    }

    property(const self& other) noexcept : property_base(other)
    {
        val = other.val;
        func = details::default_notifier<T>();
    }

    self& operator=(const self& other)
    {
        property_base::operator=(other);
        val = other.val;
        func = details::default_notifier<T>();

        return *this;
    }

    self& operator=(self&& other)
    {
        property_base::operator=(std::move(other));
        val = other.val;
        func = std::move(other.func);

        return *this;
    }

    self& operator=(const_reference val)
    {
        if (is_owner()) {
            set_directly_as_owner(val);
        }

        return *this;
    }

    operator value_type() const { return value(); }

    const_reference value() const
    {
        if (details::is_currently_binding()) {
            details::register_property(const_cast<self*>(this));
        }
        return val;
    }

    void set(const_reference val)
    {
        if (!is_owner()) {
            if (owner) {
                owner_casted()->set_using_setter_as_owner(val);
            }
        } else {
            set_directly_as_owner(val);
        }
    }

    void request_change(const_reference val)
    {
        if (owner) {
            owner_casted()->set_using_setter_as_owner(val);
        }
    }

    void become_owner()
    {
        detach();
        owner = this;
        func = details::default_setter<T>{};
    }

    template <typename Lambda>
    bool set_setter(Lambda lambda)
    {
        if (!is_owner())
            return false;

        func = details::property_setter<T, decltype(lambda)>{lambda};

        return true;
    }

    template <typename Lambda>
    bool set_notifier(Lambda lambda)
    {
        func = details::property_notifier<T, decltype(lambda)>{lambda};

        return true;
    }

    template <typename BindingLambda, typename SetterLambda = details::nop,
              typename NotifierLambda = details::nop>
    bool set_binding(BindingLambda binding_lambda,
                     SetterLambda setter_lambda = details::nop{},
                     NotifierLambda notification_lambda = details::nop{})
    {
        if (!is_owner())
            return false;

        func = details::property_binder<T, BindingLambda, SetterLambda,
                                        NotifierLambda>{
            binding_lambda, setter_lambda, notification_lambda};

        func(this, nullptr, details::call_type::initial_binding);
        return true;
    }

private:
    self* owner_casted() { return static_cast<self*>(this->owner); }

    void set_using_setter_as_owner(const_reference new_val)
    {
        if (val != new_val) {
            func(this, (void*)&new_val, details::call_type::setter);
        }
    }
    void set_directly_as_owner(const_reference new_val)
    {
        if (val != new_val) {
            val = new_val;
            notify_all(&val);
        }
    }

private:
    T val;
};

} // namespace bindable_properties

#endif // BINDABLE_PROPERTIES_H
