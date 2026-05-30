#pragma once
#include "utils/string_utils.h"

// based on https://vkguide.dev/docs/extra-chapter/cvar_system/

// global definition
// checkbox CVAR
// AutoCVar_Int CVAR_TestCheckbox("test.checkbox", "just a checkbox", 0, CVarFlags::EditCheckbox);
//
// // int CVAR
// AutoCVar_Int CVAR_TestInt("test.int", "just a configurable int", 42);
//
// // float CVAR
// AutoCVar_Int CVAR_TestFloat("test.float", "just a configurable float", 13.37);
//
// // string CVAR
// AutoCVar_String CVAR_TestString("test.string", "just a configurable string", "just a configurable string");


namespace Momo_Cvars
{
    class CVarParameter;

    enum class CVarFlags : uint32_t
    {
        None = 0,
        NoEdit = 1 << 1,
        EditReadOnly = 1 << 2,
        Advanced = 1 << 3,

        EditCheckbox = 1 << 8,
        EditFloatDrag = 1 << 9,
    };

    // interface for pImpl pattern.
    class CVarSystem
    {
    public:
        virtual ~CVarSystem() = default;
        static CVarSystem* Get();

        // pimpl
        virtual CVarParameter* GetCVar(Momo_StringUtils::StringHash aHash) = 0;

        virtual double* GetFloatCVar(Momo_StringUtils::StringHash aHash) = 0;

        virtual int32_t* GetIntCVar(Momo_StringUtils::StringHash aHash) = 0;

        virtual const char* GetStringCVar(Momo_StringUtils::StringHash aHash) = 0;

        virtual void SetFloatCVar(Momo_StringUtils::StringHash aHash, double aValue) = 0;

        virtual void SetIntCVar(Momo_StringUtils::StringHash aHash, int32_t aValue) = 0;

        virtual void SetStringCVar(Momo_StringUtils::StringHash aHash, const char* aValue) = 0;


        virtual CVarParameter* CreateFloatCVar(const char* aName, const char* aDescription, double aDefaultValue, double aCurrentValue) = 0;

        virtual CVarParameter* CreateIntCVar(const char* aName, const char* aDescription, int32_t aDefaultValue, int32_t aCurrentValue) = 0;

        virtual CVarParameter* CreateStringCVar(const char* aName, const char* aDescription, const char* aDefaultValue, const char* aCurrentValue) = 0;

        virtual void DrawImGuiEditor() = 0;
    };

    template <typename T>
    struct AutoCVar
    {
    protected:
        int _index = 0;
        using CVarType = T;
    };

    struct AutoCVar_Float : AutoCVar<double>
    {
        AutoCVar_Float(const char* aName, const char* aDescription, double aDefaultValue, CVarFlags aFlags = CVarFlags::None);

        double Get() const;
        double* GetPtr() const;
        float GetFloat() const;
        void Set(double aVal) const;
    };

    struct AutoCVar_Int : AutoCVar<int32_t>
    {
        AutoCVar_Int(const char* aName, const char* aDescription, int32_t aDefaultValue, CVarFlags aFlags = CVarFlags::None);
        int32_t Get() const;
        int32_t* GetPtr() const;
        void Set(int32_t aVal) const;

        void Toggle() const;
    };

    struct AutoCVar_String : AutoCVar<std::string>
    {
        AutoCVar_String(const char* aName, const char* aDescription, const char* aDefaultValue, CVarFlags aFlags = CVarFlags::None);

        const char* Get() const;
        void Set(std::string&& aVal) const;
    };
}
