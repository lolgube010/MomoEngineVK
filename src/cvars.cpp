#include "cvars.h"

#include <unordered_map>

#include <algorithm>
#include <cassert>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui/imgui_internal.h>
#include <shared_mutex>

namespace momo_cvars
{
    namespace
    {
        enum class CVarType : char
        {
            Undefined,
            Int,
            Float,
            String
        };

        template<typename T>
        struct CVarStorage
        {
            T _initial;
            T _current;
            CVarParameter* _parameter;
        };
    }

    class CVarParameter
    {
    public:
        friend class CVarSystemImpl;
        
        int32_t _arrayIndex;

        CVarType _type;
        CVarFlags _flags;
        std::string _name;
        std::string _description;
    };


    template<typename T>
    struct CVarArray
    {
        CVarStorage<T>* _cvars = nullptr;
        int32_t _lastCVar = 0;

        explicit CVarArray(const size_t aSize)
        {
            _cvars = new CVarStorage<T>[aSize]();
        }
        ~CVarArray() { delete[] _cvars; }

        CVarArray(const CVarArray&) = delete;
        CVarArray& operator=(const CVarArray&) = delete;
        CVarArray(CVarArray&&) = delete;
        CVarArray& operator=(CVarArray&&) = delete;

        T* GetCurrentPtr(int32_t aIndex)
        {
            return &_cvars[aIndex]._current;
        }

        T GetCurrent(int32_t aIndex)
        {
            return _cvars[aIndex]._current;
        }

        void SetCurrent(const T& aVal, int32_t aIndex)
        {
            _cvars[aIndex]._current = aVal;
        }

        int Add(const T& aValue, CVarParameter* aParam)
        {
            int index = _lastCVar;

            _cvars[index]._current = aValue;
            _cvars[index]._initial = aValue;
            _cvars[index]._parameter = aParam;

            aParam->_arrayIndex = index;
            _lastCVar++;
            return index;
        }

        int Add(const T& aInitialValue, const T& aCurrentValue, CVarParameter* aParam)
        {
            int index = _lastCVar;

            _cvars[index]._current = aCurrentValue;
            _cvars[index]._initial = aInitialValue;
            _cvars[index]._parameter = aParam;

            aParam->_arrayIndex = index;
            _lastCVar++;
            return index;
        }
    };

    class CVarSystemImpl : public CVarSystem
    {
    public:
        CVarParameter* GetCVar(momo_stringUtils::StringHash aHash) final;
        
        CVarParameter* CreateFloatCVar(const char* aName, const char* aDescription, double aDefaultValue, double aCurrentValue) final;
        CVarParameter* CreateIntCVar(const char* aName, const char* aDescription, int32_t aDefaultValue, int32_t aCurrentValue) final;
        CVarParameter* CreateStringCVar(const char* aName, const char* aDescription, const char* aDefaultValue, const char* aCurrentValue) final;

        double* GetFloatCVar(momo_stringUtils::StringHash aHash) final;
        int32_t* GetIntCVar(momo_stringUtils::StringHash aHash) final;
        const char* GetStringCVar(momo_stringUtils::StringHash aHash) final;


        void SetFloatCVar(momo_stringUtils::StringHash aHash, double aValue) final;
        void SetIntCVar(momo_stringUtils::StringHash aHash, int32_t aValue) final;
        void SetStringCVar(momo_stringUtils::StringHash aHash, const char* aValue) final;

        void DrawImGuiEditor() final;

        void EditParameter(CVarParameter* aP, float aTextWidth);

        constexpr static int MAX_INT_CVARS = 1000;
        CVarArray<int32_t> _intCVars{MAX_INT_CVARS};

        constexpr static int MAX_FLOAT_CVARS = 1000;
        CVarArray<double> _floatCVars{MAX_FLOAT_CVARS};

        constexpr static int MAX_STRING_CVARS = 200;
        CVarArray<std::string> _stringCVars{MAX_STRING_CVARS};

        // using templates with specializations to get the cvar arrays for each type.
        // if you try to use a type that doesnt have specialization, it will trigger a linker error
        template <typename T>
        CVarArray<T>* GetCVarArray();

        template <>
        CVarArray<int32_t>* GetCVarArray()
        {
            return &_intCVars;
        }
        template <>
        CVarArray<double>* GetCVarArray()
        {
            return &_floatCVars;
        }
        template <>
        CVarArray<std::string>* GetCVarArray()
        {
            return &_stringCVars;
        }

        // templated get-set cvar versions for syntax sugar
        template <typename T>
        T* GetCVarCurrent(const uint32_t aNameHash)
        {
            if (CVarParameter* par = GetCVar(static_cast<momo_stringUtils::StringHash>(aNameHash)); 
                !par)
            {
                return nullptr;
            }
            else
            {
                return GetCVarArray<T>()->GetCurrentPtr(par->_arrayIndex);
            }
        }

        template <typename T>
        void SetCVarCurrent(const uint32_t aNameHash, const T& aValue)
        {
            if (CVarParameter* cvar = GetCVar(static_cast<momo_stringUtils::StringHash>(aNameHash)))
            {
                GetCVarArray<T>()->SetCurrent(aValue, cvar->_arrayIndex);
            }
        }

        static CVarSystemImpl* Get() { return dynamic_cast<CVarSystemImpl*>(CVarSystem::Get()); }

    private:
        std::shared_mutex _mutex_;

        CVarParameter* InitCVar(const char* aName, const char* aDescription);

        std::unordered_map<uint32_t, CVarParameter> _savedCVars;

        std::vector<CVarParameter*> _cachedEditParameters;
    };

double* CVarSystemImpl::GetFloatCVar(const momo_stringUtils::StringHash aHash) {return GetCVarCurrent<double>(static_cast<uint32_t>(aHash)); }

    int32_t* CVarSystemImpl::GetIntCVar(const momo_stringUtils::StringHash aHash) {return GetCVarCurrent<int32_t>(static_cast<uint32_t>(aHash)); }

    const char* CVarSystemImpl::GetStringCVar(const momo_stringUtils::StringHash aHash)
    {
        const std::string* str = GetCVarCurrent<std::string>(static_cast<uint32_t>(aHash));
        return str ? str->c_str() : nullptr;
    }


    CVarSystem* CVarSystem::Get()
    {
        static CVarSystemImpl cvarSys{};
        return &cvarSys;
    }


    CVarParameter* CVarSystemImpl::GetCVar(const momo_stringUtils::StringHash aHash)
    {
        std::shared_lock lock(_mutex_);

        if (const auto it = _savedCVars.find(static_cast<uint32_t>(aHash)); it != _savedCVars.end())
        {
            return &it->second;
        }

        return nullptr;
    }

    void CVarSystemImpl::SetFloatCVar(const momo_stringUtils::StringHash aHash, const double aValue) {SetCVarCurrent<double>(static_cast<uint32_t>(aHash), aValue); }

    void CVarSystemImpl::SetIntCVar(const momo_stringUtils::StringHash aHash, const int32_t aValue) {SetCVarCurrent<int32_t>(static_cast<uint32_t>(aHash), aValue); }

    void CVarSystemImpl::SetStringCVar(const momo_stringUtils::StringHash aHash, const char* aValue) {SetCVarCurrent<std::string>(static_cast<uint32_t>(aHash), aValue); }


    CVarParameter* CVarSystemImpl::CreateFloatCVar(const char* aName, const char* aDescription, const double aDefaultValue, const double aCurrentValue)
    {
        std::unique_lock lock(_mutex_);
        CVarParameter* param = InitCVar(aName, aDescription);
        if (!param)
            return nullptr;

        param->_type = CVarType::Float;

        GetCVarArray<double>()->Add(aDefaultValue, aCurrentValue, param);

        return param;
    }


    CVarParameter* CVarSystemImpl::CreateIntCVar(const char* aName, const char* aDescription, const int32_t aDefaultValue, const int32_t aCurrentValue)
    {
        std::unique_lock lock(_mutex_);
        CVarParameter* param = InitCVar(aName, aDescription);
        if (!param)
            return nullptr;

        param->_type = CVarType::Int;

        GetCVarArray<int32_t>()->Add(aDefaultValue, aCurrentValue, param);

        return param;
    }


    CVarParameter* CVarSystemImpl::CreateStringCVar(const char* aName, const char* aDescription, const char* aDefaultValue, const char* aCurrentValue)
    {
        std::unique_lock lock(_mutex_);
        CVarParameter* param = InitCVar(aName, aDescription);
        if (!param)
            return nullptr;

        param->_type = CVarType::String;

        GetCVarArray<std::string>()->Add(aDefaultValue, aCurrentValue, param);

        return param;
    }

    CVarParameter* CVarSystemImpl::InitCVar(const char* aName, const char* aDescription)
    {
        const uint32_t nameHash = static_cast<uint32_t>(momo_stringUtils::StringHash{aName});
        if (_savedCVars.contains(nameHash))
            return nullptr;

        _savedCVars[nameHash] = CVarParameter{};

        CVarParameter& newParam = _savedCVars[nameHash];

        newParam._name = aName;
        newParam._description = aDescription;

        return &newParam;
    }

    AutoCVar_Float::AutoCVar_Float(const char* aName, const char* aDescription, const double aDefaultValue, const CVarFlags aFlags)
    {
        CVarParameter* cvar = CVarSystem::Get()->CreateFloatCVar(aName, aDescription, aDefaultValue, aDefaultValue);
        assert(cvar != nullptr && "CVar name already registered");
        cvar->_flags = aFlags;
        _index = cvar->_arrayIndex;
    }

    namespace
    {
        template <typename T>
        T get_c_var_current_by_index(int32_t aIndex)
        {
            return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrent(aIndex);
        }
        template <typename T>
        T* ptr_get_c_var_current_by_index(int32_t aIndex)
        {
            return CVarSystemImpl::Get()->GetCVarArray<T>()->GetCurrentPtr(aIndex);
        }
    
        template <typename T>
        void set_c_var_current_by_index(int32_t aIndex, const T& aData)
        {
            CVarSystemImpl::Get()->GetCVarArray<T>()->SetCurrent(aData, aIndex);
        }
    }




    double AutoCVar_Float::Get() const
    {
        return get_c_var_current_by_index<CVarType>(_index);
    }

    double* AutoCVar_Float::GetPtr() const
    {   
        return ptr_get_c_var_current_by_index<CVarType>(_index);
    }   

    float AutoCVar_Float::GetFloat() const { return static_cast<float>(Get()); }

    void AutoCVar_Float::Set(const double aVal) const { set_c_var_current_by_index<CVarType>(_index, aVal); }

    AutoCVar_Int::AutoCVar_Int(const char* aName, const char* aDescription, const int32_t aDefaultValue, const CVarFlags aFlags)
    {
        CVarParameter* cvar = CVarSystem::Get()->CreateIntCVar(aName, aDescription, aDefaultValue, aDefaultValue);
        assert(cvar != nullptr && "CVar name already registered");
        cvar->_flags = aFlags;
        _index = cvar->_arrayIndex;
    }

    int32_t AutoCVar_Int::Get() const { return get_c_var_current_by_index<CVarType>(_index); }

    int32_t* AutoCVar_Int::GetPtr() const { return ptr_get_c_var_current_by_index<CVarType>(_index); }

    void AutoCVar_Int::Set(const int32_t aVal) const { set_c_var_current_by_index<CVarType>(_index, aVal); }

    void AutoCVar_Int::Toggle() const
    {
        const bool enabled = Get() != 0;

        Set(enabled ? 0 : 1);
    }

    AutoCVar_String::AutoCVar_String(const char* aName, const char* aDescription, const char* aDefaultValue, const CVarFlags aFlags)
    {
        CVarParameter* cvar = CVarSystem::Get()->CreateStringCVar(aName, aDescription, aDefaultValue, aDefaultValue);
        assert(cvar != nullptr && "CVar name already registered");
        cvar->_flags = aFlags;
        _index = cvar->_arrayIndex;
    }

    const char* AutoCVar_String::Get() const { return ptr_get_c_var_current_by_index<CVarType>(_index)->c_str(); }

    void AutoCVar_String::Set(std::string&& val) const { set_c_var_current_by_index<CVarType>(_index, val); }


    void CVarSystemImpl::DrawImGuiEditor()
    {
        static std::string searchText;

        ImGui::InputText("Filter", &searchText);
        static bool bShowAdvanced = false;
        ImGui::Checkbox("Advanced", &bShowAdvanced);
        ImGui::Separator();
        _cachedEditParameters.clear();

        auto add_to_edit_list = [&](auto aParameter)
        {
            const bool bHidden = (static_cast<uint32_t>(aParameter->_flags) & static_cast<uint32_t>(CVarFlags::NoEdit));
            const bool bIsAdvanced = (static_cast<uint32_t>(aParameter->_flags) & static_cast<uint32_t>(CVarFlags::Advanced));

            if (!bHidden)
            {
                if (!(!bShowAdvanced && bIsAdvanced) && aParameter->_name.find(searchText) != std::string::npos)
                {
                    _cachedEditParameters.push_back(aParameter);
                };
            }
        };

        for (int i = 0; i < GetCVarArray<int32_t>()->_lastCVar; i++)
        {
            add_to_edit_list(GetCVarArray<int32_t>()->_cvars[i]._parameter);
        }
        for (int i = 0; i < GetCVarArray<double>()->_lastCVar; i++)
        {
            add_to_edit_list(GetCVarArray<double>()->_cvars[i]._parameter);
        }
        for (int i = 0; i < GetCVarArray<std::string>()->_lastCVar; i++)
        {
            add_to_edit_list(GetCVarArray<std::string>()->_cvars[i]._parameter);
        }

        if (_cachedEditParameters.size() > 10)
        {
            std::unordered_map<std::string, std::vector<CVarParameter*>> categorizedParams;

            // insert all the edit parameters into the hashmap by category
            for (auto p : _cachedEditParameters)
            {
                int dotPos = -1;
                // find where the first dot is to categorize
                for (int i = 0; i < p->_name.length(); i++)
                {
                    if (p->_name[i] == '.')
                    {
                        dotPos = i;
                        break;
                    }
                }
                std::string category;
                if (dotPos != -1)
                {
                    category = p->_name.substr(0, dotPos);
                }

                categorizedParams[category].push_back(p);
            }

            for (auto& [category, parameters] : categorizedParams)
            {
                // alphabetical sort
                std::ranges::sort(parameters, [](const CVarParameter* aA, const CVarParameter* aB) { return aA->_name < aB->_name; });

                if (ImGui::BeginMenu(category.c_str()))
                {
                    float maxTextWidth = 0;

                    for (const auto p : parameters)
                    {
                        maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->_name.c_str()).x);
                    }
                    for (const auto p : parameters)
                    {
                        EditParameter(p, maxTextWidth);
                    }

                    ImGui::EndMenu();
                }
            }
        }
        else
        {
            // alphabetical sort
            std::ranges::sort(_cachedEditParameters, [](const CVarParameter* aA, const CVarParameter* aB) { return aA->_name < aB->_name; });
            float maxTextWidth = 0;
            for (const auto p : _cachedEditParameters)
            {
                maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(p->_name.c_str()).x);
            }
            for (const auto p : _cachedEditParameters)
            {
                EditParameter(p, maxTextWidth);
            }
        }
    }

    static void label(const char* aLabel, const float aTextWidth)
    {
        constexpr float slack = 50;
        constexpr float editorWidth = 100;

        const float fullWidth = aTextWidth + slack;

        const ImVec2 startPos = ImGui::GetCursorScreenPos();

        ImGui::Text(aLabel);

        const ImVec2 finalPos = {startPos.x + fullWidth, startPos.y};

        ImGui::SameLine();
        ImGui::SetCursorScreenPos(finalPos);

        ImGui::SetNextItemWidth(editorWidth);
    }
    void CVarSystemImpl::EditParameter(CVarParameter* aP, const float aTextWidth)
    {
        const bool readonlyFlag = (static_cast<uint32_t>(aP->_flags) & static_cast<uint32_t>(CVarFlags::EditReadOnly));
        const bool checkboxFlag = (static_cast<uint32_t>(aP->_flags) & static_cast<uint32_t>(CVarFlags::EditCheckbox));
        const bool dragFlag = (static_cast<uint32_t>(aP->_flags) & static_cast<uint32_t>(CVarFlags::EditFloatDrag));


        switch (aP->_type)
        {
        case CVarType::Int:

            if (readonlyFlag)
            {
                ImGui::Text("%s= %i", aP->_name.c_str(), GetCVarArray<int32_t>()->GetCurrent(aP->_arrayIndex));
            }
            else
            {
                if (checkboxFlag)
                {
                    bool bCheckbox = GetCVarArray<int32_t>()->GetCurrent(aP->_arrayIndex) != 0;
                    label(aP->_name.c_str(), aTextWidth);

                    ImGui::PushID(aP->_name.c_str());

                    if (ImGui::Checkbox("", &bCheckbox))
                    {
                        GetCVarArray<int32_t>()->SetCurrent(bCheckbox ? 1 : 0, aP->_arrayIndex);
                    }
                    ImGui::PopID();
                }
                else
                {
                    label(aP->_name.c_str(), aTextWidth);
                    ImGui::PushID(aP->_name.c_str());
                    ImGui::InputInt("", GetCVarArray<int32_t>()->GetCurrentPtr(aP->_arrayIndex));
                    ImGui::PopID();
                }
            }
            break;

        case CVarType::Float:

            if (readonlyFlag)
            {
                ImGui::Text("%s= %f", aP->_name.c_str(), GetCVarArray<double>()->GetCurrent(aP->_arrayIndex));
            }
            else
            {
                label(aP->_name.c_str(), aTextWidth);
                ImGui::PushID(aP->_name.c_str());
                if (dragFlag)
                {
                    ImGui::DragScalar("", ImGuiDataType_Double, GetCVarArray<double>()->GetCurrentPtr(aP->_arrayIndex), 0.1f, nullptr, nullptr, "%.3f");
                }
                else
                {
                    ImGui::InputDouble("", GetCVarArray<double>()->GetCurrentPtr(aP->_arrayIndex), 0, 0, "%.3f");
                }
                ImGui::PopID();
            }
            break;

        case CVarType::String:

            if (readonlyFlag)
            {
                ImGui::PushID(aP->_name.c_str());
                ImGui::Text("%s= %s", aP->_name.c_str(), GetCVarArray<std::string>()->GetCurrentPtr(aP->_arrayIndex)->c_str());

                ImGui::PopID();
            }
            else
            {
                label(aP->_name.c_str(), aTextWidth);
                ImGui::PushID(aP->_name.c_str());
                ImGui::InputText("", GetCVarArray<std::string>()->GetCurrentPtr(aP->_arrayIndex));
                ImGui::PopID();
            }
            break;

        default:
            break;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(aP->_description.c_str());
        }
    }
}
