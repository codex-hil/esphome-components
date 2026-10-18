#pragma once
#include <functional>
namespace esphome {
template<typename... Ts> class Action { public: virtual ~Action()=default; virtual void play(const Ts &...)=0; };
template<typename T, typename... Ts> class TemplatableValue { public:
 T value(Ts... xs) { return fn ? fn(xs...) : v; } T v{}; std::function<T(Ts...)> fn;
 void set(T x) { v=x; } void set(std::function<T(Ts...)> x) { fn=x; }
};
}
#define TEMPLATABLE_VALUE(type, name) protected: esphome::TemplatableValue<type, Ts...> name##_; public: template<typename V> void set_##name(V v) { name##_.set(v); }
