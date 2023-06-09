#include <cmath>
#include <gtest/gtest.h>
#include <string>

#include "bindable_properties.h"

using MyTypes = ::testing::Types<int, long, std::string>;
template <typename T>
class Tests : public testing::Test
{
};
TYPED_TEST_SUITE(Tests, MyTypes);

// pretty much std::iota but on the byte level
template <typename T>
inline T new_value(int idx)
{
    T data;
    char* ptr = (char*)&data;
    int cur = idx;
    char* cur_bytes = (char*)(&cur);
    int cur_bytes_idx = 0;

    for (int i = 0; i < sizeof(data); i++) {
        ptr[i] = cur_bytes[cur_bytes_idx++];

        if (cur_bytes_idx == sizeof(cur)) {
            cur++;
            cur_bytes_idx = 0;
        }
    }

    return data;
}

template <>
inline std::string new_value<std::string>(int idx)
{
    return std::string("this is a string having the index number: ") +
           std::to_string(idx);
}

namespace bp = bindable_properties;

TYPED_TEST(Tests, CopyingAPropertyCreatesAView)
{
    TypeParam v1 = new_value<TypeParam>(123);
    TypeParam v2 = new_value<TypeParam>(200);

    ASSERT_NE(v1, v2);

    bp::property<TypeParam> prop = v1;
    bp::property<TypeParam> view_prop = prop;

    EXPECT_TRUE(prop.is_owner());
    EXPECT_TRUE(view_prop.is_view());
    EXPECT_EQ(prop.value(), view_prop.value());
    EXPECT_EQ(prop.value(), v1);
    EXPECT_EQ(view_prop.value(), v1);

    prop = v2;
    EXPECT_TRUE(prop.is_owner());
    EXPECT_TRUE(view_prop.is_view());
    EXPECT_EQ(prop.value(), view_prop.value());
    EXPECT_EQ(prop.value(), v2);
    EXPECT_EQ(view_prop.value(), v2);
}

TYPED_TEST(Tests, CopyingAPropertyCreatesAViewManyViews)
{
    static constexpr int NUM_VIEWS = 1024;

    TypeParam v1 = new_value<TypeParam>(123);
    TypeParam v2 = new_value<TypeParam>(200);

    ASSERT_NE(v1, v2);

    bp::property<TypeParam> prop = v1;

    std::vector<bp::property<TypeParam>> view_props;
    for (int i = 0; i < NUM_VIEWS / 3; i++) {
        bp::property<TypeParam> view_prop = prop;
        view_props.push_back(view_prop);
    }
    for (int i = 0; i < NUM_VIEWS / 3; i++) {
        bp::property<TypeParam> view_prop = view_props.back();
        view_props.push_back(view_prop);
    }
    for (int i = 0; i < NUM_VIEWS / 3; i++) {
        bp::property<TypeParam> view_prop = view_props.front();
        view_props.push_back(view_prop);
    }

    EXPECT_TRUE(prop.is_owner());
    for (auto& view_prop : view_props) {
        EXPECT_TRUE(view_prop.is_view());
        EXPECT_EQ(prop.value(), view_prop.value());
        EXPECT_EQ(prop.value(), v1);
        EXPECT_EQ(view_prop.value(), v1);
    }

    prop = v2;
    EXPECT_TRUE(prop.is_owner());
    for (auto& view_prop : view_props) {
        EXPECT_TRUE(view_prop.is_view());
        EXPECT_EQ(prop.value(), view_prop.value());
        EXPECT_EQ(prop.value(), v2);
        EXPECT_EQ(view_prop.value(), v2);
    }
}

TYPED_TEST(Tests, BasicSetterAndNotifier)
{
    TypeParam value = new_value<TypeParam>(123);

    bp::property<TypeParam> prop;
    bp::property<TypeParam> view = prop;

    EXPECT_TRUE(
        prop.set_setter([&](bp::property<TypeParam>& prop2, TypeParam value) {
            EXPECT_EQ(std::addressof(prop), std::addressof(prop2));
            EXPECT_TRUE(prop2.is_owner());
            prop2 = value;
        }));

    bool notificationReceived = false;

    EXPECT_FALSE(view.set_setter([]() {}));
    EXPECT_TRUE(view.set_notifier([&](const TypeParam& new_value) {
        notificationReceived = true;
        EXPECT_EQ(new_value, value);
    }));

    view.request_change(value);

    EXPECT_TRUE(notificationReceived);
    EXPECT_EQ(prop.value(), view.value());
    EXPECT_EQ(prop.value(), value);
    EXPECT_EQ(view.value(), value);
}

TYPED_TEST(Tests, OwnerCanHaveANotifierInsteadOfSetter)
{
    TypeParam value = new_value<TypeParam>(123);
    bp::property<TypeParam> prop;

    EXPECT_TRUE(prop.is_owner());

    bool notificationReceived = false;

    EXPECT_TRUE(prop.set_notifier([&](const TypeParam& new_value) {
        notificationReceived = true;
        EXPECT_EQ(new_value, value);
    }));

    prop.request_change(value);

    EXPECT_TRUE(notificationReceived);
    EXPECT_EQ(prop.value(), value);
}

TYPED_TEST(Tests, BindingsSingleProp)
{
    TypeParam value = new_value<TypeParam>(123);
    TypeParam value_x_2 = value + value;
    TypeParam value_x_3 = value + value + value;

    ASSERT_NE(value, value_x_2);

    bp::property<TypeParam> prop;
    bp::property<TypeParam> bound_prop;

    // try with value capture
    bound_prop.set_binding([=]() { return prop.value() + prop.value(); });

    prop = value;

    EXPECT_EQ(prop.value(), value);
    EXPECT_EQ(bound_prop.value(), value_x_2);

    // try with reference capture
    bound_prop.set_binding(
        [&]() { return prop.value() + prop.value() + prop.value(); });

    EXPECT_EQ(prop.value(), value);
    EXPECT_EQ(bound_prop.value(), value_x_3);
}

TYPED_TEST(Tests, BindingsMultipleProps)
{
    TypeParam value1 = new_value<TypeParam>(123);
    TypeParam value2 = new_value<TypeParam>(223);
    TypeParam value3 = new_value<TypeParam>(323);

    ASSERT_NE(value1, value2);
    ASSERT_NE(value1, value3);
    ASSERT_NE(value2, value3);

    bp::property<TypeParam> prop1;
    bp::property<TypeParam> prop2;
    bp::property<TypeParam> prop3;
    bp::property<TypeParam> bound_prop;

    bound_prop.set_binding(
        [=]() { return prop1.value() + prop2.value() + prop3.value(); });

    prop1 = value1;
    EXPECT_EQ(bound_prop.value(), value1 + prop2.value() + prop3.value());

    prop2 = value2;
    EXPECT_EQ(bound_prop.value(), value1 + value2 + prop3.value());

    prop3 = value3;
    EXPECT_EQ(bound_prop.value(), value1 + value2 + value3);
}

TYPED_TEST(Tests, BindingsMultiplePropsTwoLayers)
{
    TypeParam value1 = new_value<TypeParam>(123);
    TypeParam value2 = new_value<TypeParam>(223);
    TypeParam value3 = new_value<TypeParam>(323);

    ASSERT_NE(value1, value2);
    ASSERT_NE(value1, value3);
    ASSERT_NE(value2, value3);

    bp::property<TypeParam> prop1;
    bp::property<TypeParam> prop2;
    bp::property<TypeParam> prop3;
    bp::property<TypeParam> bound_prop;
    bp::property<TypeParam> bound_prop2;

    bound_prop.set_binding(
        [=]() { return prop1.value() + prop2.value() + prop3.value(); });
    bound_prop2.set_binding(
        [=]() { return bound_prop.value() + bound_prop.value(); });

    prop1 = value1;
    prop2 = value2;
    prop3 = value3;

    EXPECT_EQ(bound_prop.value(), value1 + value2 + value3);
    EXPECT_EQ(bound_prop2.value(), bound_prop.value() + bound_prop.value());
}

TYPED_TEST(Tests, MoveSemantics)
{
    TypeParam value1 = new_value<TypeParam>(123);
    TypeParam value2 = new_value<TypeParam>(223);
    TypeParam value3 = new_value<TypeParam>(323);

    ASSERT_NE(value1, value2);
    ASSERT_NE(value1, value3);
    ASSERT_NE(value2, value3);

    bp::property<TypeParam> x = value1;
    bp::property<TypeParam> y = x;

    EXPECT_EQ(y.value(), value1);

    bp::property<TypeParam> z = std::move(x);

    EXPECT_EQ(z.value(), value1);

    z = value2;

    EXPECT_EQ(z.value(), value2);
    EXPECT_EQ(y.value(), value2);

    bp::property<TypeParam> w;
    w = std::move(z);

    w = value3;
    EXPECT_EQ(w.value(), value3);
    EXPECT_EQ(y.value(), value3);
}

TEST(Tests, PythagorasExample)
{
    bp::property<int> x = 20;
    bp::property<int> y = 21;

    bp::property<double> hypotenuse;

    int notificationsReceived = 0;
    double receivedValues[2];

    hypotenuse.set_binding(
        [&] { return std::sqrt(x * x + y * y); },
        [&](double value) {
            y.request_change((int)std::sqrt(value * value - x * x));
        },
        [&](double value) {
            receivedValues[notificationsReceived % 2] = value;
            notificationsReceived++;
        });

    EXPECT_EQ(hypotenuse.value(), 29.0);

    hypotenuse.request_change(101.0);

    EXPECT_EQ(x.value(), 20);
    EXPECT_EQ(y.value(), 99);
    EXPECT_EQ(hypotenuse.value(), 101.0);
    EXPECT_EQ(notificationsReceived, 2);
    EXPECT_EQ(receivedValues[0], 29.0);
    EXPECT_EQ(receivedValues[1], 101.0);
}
