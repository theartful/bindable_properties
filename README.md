# Bindable Properties for C++11

This is a C++ library that provides an implementation of properties that can
be bound to each other. It allows for properties to be treated as data members,
while also providing the ability to automatically update other properties when a
property's value is changed.

## Features

- Support simple property views and complex bindings to one or more properties.
- Automatic updates for bound properties.
- Define custom notifications for when property changes its value.
- Views and bound properties can request the original property to change.

## Semantics

### Property Ownership

There are two ways to create a property: as an owner, or as a view/binding. When
you create a property as an owner, the property owns its data, and is responsible
for handling change requests. When you create a property as a view or a binding,
the property value is a function of one or more other properties, and any changes
made to these properties will be reflected in the view.

You can transfer ownership of a property to another property by using the move
constructor or move assignment operator.
```C++
property<int> x = 5;
property<int> y = x; // now y is a view of x, any changes to x will be reflected in y

property<int> z = std::move(x); // x is moved into z, so any changes to z is reflected in y
```

### Property Binding

There are two types of property bindings:
1. Views, or simple bindings. These can be done using the copy constructor.
These are lightweight and don't require any allocations.
```C++
property<int> x;
property<int> y = x;

x = 4;
assert(y.value() == 4);
```

2. Complex bindings, where a property is a function of one or more other properties.
Bound properties are automatically updated when any of their dependencies change
value.
```C++
property<int> x;
property<int> y;

property<int> z;
z.set_binding([=]() { return x.value() + y.value(); });

x = 4;
y = 5;

assert_eq(z.value() == 9);
```


### Notifications

You can register a single notification lambda function to be called whenever a
property's value changes. Notifications can only be set on bound properties,
since owner properties are the ones making the change.
```C++
x.set_notifier([](int new_value) { std::cout << "x changed to " << new_value; });
```


### Change Requests and Setters

Bound properties can request owner properties to change their values. Owner
properties respond to this request by calling the registered `setter` lambda,
or simply accept the change if no lambda is registered.
```C++
property<int> owner;
owner.set_setter([&](int value) { owner = value; });

property<int> view = owner;
view.request_change(5);

assert(view.value() == 5);
```

### Example with Complex Bindings

```C++
using namespace bindable_properties;

property<int> x = 20;
property<int> y = 21;
property<double> hypotenuse; // Represents the hypotenuse of a right triangle

// Binding: hypotenuse = sqrt(x^2 + y^2)
hypotenuse.set_binding(
    // binding expression
    [&] { return std::sqrt(x * x + y * y); },
    // handles change requests by trying to change y to achieve the requested
    // hypotenuse
    [&](double value) {
        // Setter that calls request_change on the x property
        y.request_change(std::sqrt(value * value - x * x));
    },
    // notification lambda when the value changes
    [](double value) {
        std::cout << "hypotenuse changed to " << value << '\n';
    }
);

assert(hypotenuse.value() == 29.0);

hypotenuse.request_change(101.0);
assert(x.value() == 20);
assert(y.value() == 99);
assert(hypotenuse.value() == 101.0);
```

### Gotcha

On any given instance of a property, you can only call one of `set_setter`,
`set_notifier`, or `set_binding`. A setter can be only set for the owner, and
`set_binding` cannot be called on an already bound property (such as views).
This is done because the property class only keeps track of one function to
preserve memory.

## No Allocations

This library doesn't do any memory allocations as long as you:
1. Use lambdas with only a single pointer as a state.
2. Don't use complex bindings and only use property views.

## Use

Add the following lines to your CMakeLists.txt file:
```cmake
include(FetchContent)

FetchContent_Declare(
  bindable_properties
  GIT_REPOSITORY    https://github.com/theartful/bindable_properties
  GIT_TAG           master
)

FetchContent_MakeAvailable(bindable_properties)

target_link_libraries(target PUBLIC bindable_properties)
```

