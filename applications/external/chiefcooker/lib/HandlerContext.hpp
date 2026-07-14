#pragma once

#define HANDLER(handlerMethod)      bind(handlerMethod, this)
#define HANDLER_1ARG(handlerMethod) bind(handlerMethod, this, placeholders::_1)
#define HANDLER_2ARG(handlerMethod) bind(handlerMethod, this, placeholders::_1, placeholders::_2)
#define HANDLER_3ARG(handlerMethod) bind(handlerMethod, this, placeholders::_1, placeholders::_2, placeholders::_3)

template <class T>
class HandlerContext {
private:
    T handler;

public:
    HandlerContext(T handler) {
        this->handler = handler;
    }

    T GetHandler() {
        return handler;
    }
};

template <class T>
class HandlerContextExt : public HandlerContext<T> {
private:
    void* extContext;

public:
    HandlerContextExt(T handler, void* extContext) : HandlerContext<T>(handler) {
        this->extContext = extContext;
    }

    void* GetExtContext() {
        return extContext;
    }
};
