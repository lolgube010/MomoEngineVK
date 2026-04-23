// #pragma once
// #include "string_utils.h"
//
// // based on https://vkguide.dev/docs/extra-chapter/cvar_system/
//
// namespace momo_cvars
// {
//     class CVarParameter;
//
//     enum class CVarFlags : uint32_t
//     {
//         None = 0,
//         NoEdit = 1 << 1,
//         EditReadOnly = 1 << 2,
//         Advanced = 1 << 3,
//
//         EditCheckbox = 1 << 8,
//         EditFloatDrag = 1 << 9,
//     };
//
//     // interface for pImpl pattern.
//     class CVarSystem
//     {
//     public:
//         static CVarSystem* Get();
//
//         // pimpl
//         virtual CVarParameter* GetCVar(momo_stringUtils::StringHash aHash) = 0;
//
//         virtual double* GetFloatCVar(momo_stringUtils::StringHash aHash) = 0;
//
//         virtual int32_t* GetIntCVar(momo_stringUtils::StringHash aHash) = 0;
//
//         virtual const char* GetStringCVar(momo_stringUtils::StringHash aHash) = 0;
//
//         virtual void SetFloatCVar(momo_stringUtils::StringHash aHash, double aValue) = 0;
//
//         virtual void SetIntCVar(momo_stringUtils::StringHash aHash, int32_t aValue) = 0;
//
//         virtual void SetStringCVar(momo_stringUtils::StringHash aHash, const char* aValue) = 0;
//
//
//         virtual CVarParameter* CreateFloatCVar(const char* aName, const char* aDescription, double aDefaultValue, double aCurrentValue) = 0;
//
//         virtual CVarParameter* CreateIntCVar(const char* aName, const char* aDescription, int32_t aDefaultValue, int32_t aCurrentValue) = 0;
//
//         virtual CVarParameter* CreateStringCVar(const char* aName, const char* aDescription, const char* aDefaultValue, const char* aCurrentValue) = 0;
//
//         virtual void DrawImGuiEditor() = 0;
//     };
//
//     template <typename T>
//     struct AutoCVar
//     {
//     protected:
//         int _index = 0;
//         using CVarType = T;
//     };
//
//     struct AutoCVar_Float : AutoCVar<double>
//     {
//         AutoCVar_Float(const char* aName, const char* aDescription, double aDefaultValue, CVarFlags aFlags = CVarFlags::None);
//
//         double Get();
//         double* GetPtr();
//         float GetFloat();
//         float* GetFloatPtr();
//         void Set(double aVal);
//     };
//
//     struct AutoCVar_Int : AutoCVar<int32_t>
//     {
//         AutoCVar_Int(const char* aName, const char* aDescription, int32_t aDefaultValue, CVarFlags aFlags = CVarFlags::None);
//         int32_t Get();
//         int32_t* GetPtr();
//         void Set(int32_t aVal);
//
//         void Toggle();
//     };
//
//     struct AutoCVar_String : AutoCVar<std::string>
//     {
//         AutoCVar_String(const char* aName, const char* aDescription, const char* aDefaultValue, CVarFlags aFlags = CVarFlags::None);
//
//         const char* Get();
//         void Set(std::string&& aVal);
//     };
// }
