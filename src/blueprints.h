#pragma once

#include <memory>
#include <mutex>

////////////////////////////////////////////////////////////////////////////////
// CRTP - curiously recurring template pattern
// A helper namespace for common virtual and singleton type patterns implemented
// as templated structures or other compile type metaprogramming instead of relying
// on "slow" runtime virtual pointers.
////////////////////////////////////////////////////////////////////////////////

namespace blueprints
{

template <template <typename> class Base, typename Derived>
struct CRTPBase
{

    Derived* derived() { return static_cast<Derived*>(static_cast<Base<Derived>*>(this)); }

    auto& operator*() { return derived()->impl; }

};

template <typename Derived>
struct InitDestroy : CRTPBase<InitDestroy, Derived>
{
    template <typename ...Args>
    constexpr bool init(Args... args) { return (**this).init(args...); }

    template <typename ...Args>
    constexpr bool destroy(Args... args) { return (**this).destroy(args...); }
};

template <typename Derived>
struct Commitable : CRTPBase<Commitable, Derived>
{
    constexpr bool begin(int w, int h) { return (**this).begin(w, h); }

    constexpr bool commit() { return (**this).commit(); }
};

#ifdef BP_ONE_TO_RULE_THEM_ALL
namespace
{
// TBD:
//  shared pointer resource manager singleton
//  The derived singleton CRTP could add this instance to a single global resource manager

template<typename T, bool AsASharedPointer = false>
class Singleton {
public:
    static std::shared_ptr<T> get_instance() {
        std::call_once(flag, [](){ instance.reset(new T); });
        return instance;
    }

    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;
protected:
    Singleton() = default;
private:
    static std::shared_ptr<T> instance;
    static std::once_flag flag;
};

// Static member initialization outside of class definition
template<typename T, bool AsAPointer>
std::shared_ptr<T> Singleton<T, AsAPointer>::instance = nullptr;

template<typename T, bool AsAPointer>
std::once_flag Singleton<T, AsAPointer>::flag;

//// Base class without singleton behavior
//template<typename T, bool IsSingleton = false>
//struct ResourceManagerBase {
//    // ... non-singleton related code here ...
//};
//
//// Specialization for Singleton behavior
//template<typename T>
//struct ResourceManagerBase<T, true> : public Singleton<ResourceManagerBase<T, true>> {
//    friend struct Singleton<ResourceManagerBase<T, true>>;
//private:
//    // Private constructor to prevent direct construction calls with the 'new' keyword
//    ResourceManagerBase() {}
//public:
//    // ... singleton related code here ...
//};
//      template <typename Derived>
//      struct Singleton
//      {
//          static Derived& get_instance()
//          {
//              static Derived instance;
//              return instance;
//          }
//      };
} // namespace
#else
namespace
{
template <typename Derived>
struct Singleton
{
    static Derived& get_instance()
    {
        static Derived instance;
        return instance;
    }
};
} // namespace
#endif

} // namespace blueprints
