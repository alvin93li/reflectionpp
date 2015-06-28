#pragma once

#include <functional>
#include <string>
#include <memory>
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <vector>
#include <map>
#include <unordered_map>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>

#include "../visitor.hpp"
#include "../accessor.hpp"

namespace rpp {

// forward declaration
template <class Doc>
struct VisitorBSONRootDocBase;

// render a BSON (MongoDB data) object
template <class Base = VisitorBSONRootDocBase<bsoncxx::builder::basic::document>>
struct VisitorBSON: public Base {
protected:
    template <class Accessor, class T>
    void visitPtr(Accessor &accessor, T &value) {
        if (value) {
            accessor.doMemberVisit(*this, *value);
        } else {
            // TODO
        }
    }

public:
    using Base::Base;

    // pointer

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::shared_ptr<Args...> &value) {
        visitPtr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::weak_ptr<Args...> &value) {
        visitPtr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::unique_ptr<Args...> &value) {
        visitPtr(accessor, value);
    }

    template <class Accessor, class T>
    void into(Accessor &accessor, T *&value) {
        visitPtr(accessor, value);
    }

    // array

    template <class Accessor, class T, rpp_size_t size>
    void into(Accessor &accessor, T (&value)[size]) {
        this->visitArr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::array<Args...> &value) {
        this->visitArr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::deque<Args...> &value) {
        this->visitArr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::forward_list<Args...> &value) {
        this->visitArr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::list<Args...> &value) {
        this->visitArr(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::vector<Args...> &value) {
        this->visitArr(accessor, value);
    }

    // map

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::map<Args...> &value) {
        this->visitMap(accessor, value);
    }

    template <class Accessor, class... Args>
    void into(Accessor &accessor, std::unordered_map<Args...> &value) {
        this->visitMap(accessor, value);
    }

    // handle different types of accessors

    template <class... Args>
    void operator()(AccessorSimple<Args...> &accessor) {
        this->visitVal(accessor());
    }

    template <class... Args>
    void operator()(AccessorObject<Args...> &accessor) {
        this->visitObj(accessor);
    }

    template <class... Args>
    void operator()(AccessorDynamic<Args...> &accessor) {
        into(accessor, accessor());
    }
};

// hold a document entry
template <class Doc>
struct VisitorBSONDocBase: public VisitorBase<void> {
protected:
    Doc &doc;
    std::string name;

    template <class Member>
    void add(const Member &member) {
        doc.append(bsoncxx::builder::basic::kvp(name, member));
    }

public:
    VisitorBSONDocBase(Doc &_doc, const std::string &_name):
        doc{_doc}, name{_name} {}
};

// hold an array
template <class Arr>
struct VisitorBSONArrBase: public VisitorBase<void> {
protected:
    Arr &arr;

    template <class Member>
    void add(const Member &member) {
        arr.append(member);
    }

public:
    VisitorBSONArrBase(Arr &_arr):
        arr{_arr} {}
};

// build a BSON object as an item in a document or an array
template <class Base>
struct VisitorBSONItemBase: public Base {
protected:
    template <class T>
    void visitVal(const T &value) {
        this->add(value);
    }

    template <class Accessor, class T>
    void visitArr(Accessor &accessor, T &value) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        this->add([&](sub_array arr) {
            auto begin = std::begin(value);
            auto end = std::end(value);
            for (auto i = begin; i != end; ++i) {
                VisitorBSON<
                    VisitorBSONItemBase<
                        VisitorBSONArrBase<sub_array>
                    >
                > child{arr};

                accessor.doMemberVisit(child, *i);
            }
        });
    }

    template <class Accessor, class T>
    void visitMap(Accessor &accessor, T &value) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        this->add([&](sub_document doc) {
            auto begin = std::begin(value);
            auto end = std::end(value);
            for (auto i = begin; i != end; ++i) {
                VisitorBSON<
                    VisitorBSONItemBase<
                        VisitorBSONDocBase<sub_document>
                    >
                > child{doc, i->first};

                accessor.doMemberVisit(child, i->second);
            }
        });
    }

    template <class... Args>
    void visitObj(AccessorObject<Args...> &accessor) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        this->add([&](sub_document doc) {
            for (rpp_size_t i = 0; i < accessor.size(); ++i) {
                VisitorBSON<
                    VisitorBSONItemBase<
                        VisitorBSONDocBase<sub_document>
                    >
                > child{doc, accessor.getMemberName(i)};

                accessor.doMemberVisit(child, i);
            }
        });
    }

public:
    using Base::Base;
};

// build a BSON object as a root (type: document)
template <class Doc = bsoncxx::builder::basic::document>
struct VisitorBSONRootDocBase: public VisitorBase<void>, public Doc {
protected:
    template <class T>
    void visitVal(const T &value) = delete;

    template <class Accessor, class T>
    void visitArr(Accessor &accessor, T &value) = delete;

    template <class Accessor, class T>
    void visitMap(Accessor &accessor, T &value) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        auto begin = std::begin(value);
        auto end = std::end(value);
        for (auto i = begin; i != end; ++i) {
            VisitorBSON<
                VisitorBSONItemBase<
                    VisitorBSONDocBase<sub_document>
                >
            > child{*this, i->first};

            accessor.doMemberVisit(child, i->second);
        }
    }

    template <class... Args>
    void visitObj(AccessorObject<Args...> &accessor) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        for (rpp_size_t i = 0; i < accessor.size(); ++i) {
            VisitorBSON<
                VisitorBSONItemBase<
                    VisitorBSONDocBase<sub_document>
                >
            > child{*this, accessor.getMemberName(i)};

            accessor.doMemberVisit(child, i);
        }
    }
};

// build a BSON object as root (type: array)
template <class Arr = bsoncxx::builder::basic::array>
struct VisitorBSONRootArrBase: public VisitorBase<void>, public Arr {
protected:
    template <class T>
    void visitVal(const T &value) = delete;

    template <class Accessor, class T>
    void visitArr(Accessor &accessor, T &value) {
        using namespace std::placeholders;
        using namespace bsoncxx::builder::basic;

        auto begin = std::begin(value);
        auto end = std::end(value);
        for (auto i = begin; i != end; ++i) {
            VisitorBSON<
                VisitorBSONItemBase<
                    VisitorBSONArrBase<sub_array>
                >
            > child{*this};

            accessor.doMemberVisit(child, *i);
        }
    }

    template <class Accessor, class T>
    void visitMap(Accessor &accessor, T &value) = delete;

    template <class... Args>
    void visitObj(AccessorObject<Args...> &accessor) = delete;
};

}