#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <utility>
#include <memory>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <charconv>
#include <cstring>

namespace ArgumentParser {
    
class ArgBase {
public:
    virtual ~ArgBase() = default;
    virtual bool isFlag() const = 0;    
    virtual bool isHelp() const = 0;
    virtual bool isPositional() const = 0;
    virtual bool isDefault() const = 0;
    virtual bool isMultiValue() const = 0;
    virtual bool isGood() const = 0;
    virtual bool isEmpty() const = 0;
    virtual bool AddValue(std::string_view value) = 0;
    virtual std::string_view getDescription() = 0;
};

template <typename ArgType>
class Arg : public ArgBase {
public:
    Arg(char short_name, std::string_view long_name, std::string_view description, bool flag, bool help, std::function<ArgType(std::string_view)> func)
        : short_name_(short_name), long_name_(long_name), description_(description), is_flag_(flag), is_help_(help), convert_func_(func){}

    bool isFlag() const override {return is_flag_;}
    bool isHelp() const override {return is_help_;}
    bool isPositional() const override {return is_positional_;}
    bool isDefault() const override {return is_default_;}
    bool isMultiValue() const override {return is_multivalue_;}
    bool isEmpty() const override {return values_.empty();}
    bool isGood() const override {return is_good_;}

    Arg& MultiValue() {
        is_multivalue_ = true;
        return *this;
    }

    Arg& MultiValue(int value) {
        is_multivalue_ = true;
        min_count_ = value;
        return *this;
    }

    Arg& Positional() {
        is_positional_ = true;
        return *this;
    }

    std::string_view getDescription() override{
        return description_;
    }

    bool AddValue(std::string_view value) override {
        ArgType val = convert_func_(value);
        is_good_ = true;
        if constexpr (std::is_arithmetic_v<ArgType>) {
            if (min_count_ != 0 && val < min_count_) {
                return false;
            }
        }
        store_func_(val);
        return true;
    }

    ArgType GetValue(int index) {
        if (!values_.empty() && values_.size() > index) {
            return values_[index];
        }
        exit(EXIT_FAILURE);
    }

    ArgType getMinCount() {
        return min_count_;
    }

    Arg& Default(ArgType value) {
        is_default_ = true;
        store_func_(value);
        is_good_ = true;
        return *this;
    }

    Arg& StoreValue(ArgType& other_value) {
        store_func_ = [this, &other_value](ArgType value) {
            other_value = value;
        };
        is_good_ = true;
        return *this;
    }

    Arg& StoreValues(std::vector<ArgType>& other_container) {
        store_func_ = [this, &other_container](ArgType value) {
            other_container.push_back(value);
            values_.push_back(value);
        };
        return *this;
    }


private:
    char short_name_;
    std::string_view long_name_;
    std::string_view description_;
    bool is_flag_ = false;
    bool is_good_ = false;
    bool is_help_ = false;
    bool is_positional_ = false;
    bool is_multivalue_ = false;
    bool is_store_values_ = false;
    bool is_default_ = false;
    int min_count_ = 0;
    bool flag = false;
    std::vector<ArgType> values_;
    std::function<ArgType(std::string_view)> convert_func_;
    std::function<void(ArgType)> store_func_ = [this](ArgType value){
        values_.push_back(value);
    };
};

class ArgParser {
public:
    ArgParser(std::string_view program_name) : program_name_(program_name) {}

    template <typename T>
    Arg<T>& AddArgument(char short_name, std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false, std::function<T(std::string_view)> convert_func = [](std::string_view value) {
        if constexpr (std::is_arithmetic_v<T>) {
            T val;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
            if (ec != std::errc()) {
                exit(EXIT_FAILURE);
            }
            return val;
        }
    }) {
        std::unique_ptr<Arg<T>> arg = std::make_unique<Arg<T>>(short_name, long_name, description, flag, help, convert_func);
        Arg<T>& ref = *arg;
        arguments_[long_name] = std::move(arg);
        arg_names_[short_name] = long_name;
        return ref;
    }

    Arg<int>& AddIntArgument(std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<int>(' ', long_name, description, flag, help, [](std::string_view value) {
            int val;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
            if (ec != std::errc()) {
                exit(EXIT_FAILURE);
            }
            return val;
        });
    }

    Arg<int>& AddIntArgument(char short_name, std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<int>(short_name, long_name, description, flag, help, [](std::string_view value) {
            int val;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), val);
            if (ec != std::errc()) {
                exit(EXIT_FAILURE);
            }
            return val;
        });
    }

    Arg<std::string>& AddStringArgument(std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<std::string>(' ', long_name, description, flag, help, [](std::string_view value) {
            return std::string(value);
        });
    }

    Arg<std::string>& AddStringArgument(char short_name, std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<std::string>(short_name, long_name, description, flag, help, [](std::string_view value) {
            return std::string(value);
        });
    }

    Arg<bool>& AddBoolArgument(std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<bool>(' ', long_name, description, flag, help, [](std::string_view value) {
            return (strcmp(value.data(), "true") == 0 || strcmp(value.data(), "1") == 0);
        });
    }

    Arg<bool>& AddBoolArgument(char short_name, std::string_view long_name, std::string_view description = "", bool flag = false, bool help = false) {
        return AddArgument<bool>(short_name, long_name, description, flag, help, [](std::string_view value) {
            return (strcmp(value.data(), "true") == 0 || strcmp(value.data(), "1") == 0);
        });
    }

    Arg<bool>& AddFlag(char short_name, std::string_view long_name, std::string_view description = "") {
        return AddArgument<bool>(short_name, long_name, description, true, false, [](std::string_view value) {
            return true;
        });
    }

    Arg<bool>& AddFlag(std::string_view long_name, std::string_view description = "") {
        return AddArgument<bool>(' ', long_name, description, true, false, [](std::string_view value) {
            return true;
        });
    }

    Arg<bool>& AddHelp(char short_name, std::string_view long_name, std::string_view description = "Show help message") {
        return AddArgument<bool>(short_name, long_name, description, false, true, [](std::string_view value) {
            return true;
        });
    }

    bool Parse(const std::vector<std::string>& argv) {
        int argc = argv.size();
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg[0] == '-') { 
                std::string_view arg_name = (arg[1] == '-') ? arg.substr(2) : arg.substr(1);
                if (arg[1] == '-') {
                    if (arg_name.find('=') != std::string_view::npos) {
                        auto [name, value] = splitString(arg_name, '=');
                        auto it = arguments_.find(name);
                        if (it != arguments_.end()) {
                            if (!it->second->AddValue(value)) {
                                return false;
                            }
                        } else {
                            std::cerr << "Unknown argument: --" << name << "\n";
                            return false;
                        }
                    } else { 
                        auto it = arguments_.find(arg_name);
                        if (it != arguments_.end()) {
                            auto& argument = it->second;
                            if (argument->isFlag()) {
                                argument->AddValue("true");
                            } else if (argument->isHelp()) {
                                help_flag_ = true;
                                return true;
                            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                                std::string_view value = argv[++i];
                                if (!argument->AddValue(value)) {
                                    return false;
                                }
                                while (argument->isMultiValue() && i + 1 < argc && argv[i + 1][0] != '-') {
                                    if (!argument->AddValue(argv[++i])) {
                                        return false;
                                    }
                                }
                            } else {
                                std::cerr << "Missing value for option: --" << arg_name << "\n";
                                return false;
                            }
                        } else {
                            std::cerr << "Unknown argument: --" << arg_name << "\n";
                            return false;
                        }
                    }
                } else {
                    if (arg_name.find('=') != std::string_view::npos) {
                        auto [name, value] = splitString(arg_name, '=');
                        char short_name = name[0];
                        auto it = arg_names_.find(short_name);
                        if (it != arg_names_.end()) {
                            if (!arguments_[arg_names_[short_name]]->AddValue(value)) {
                                return false;
                            }
                        } else {
                            std::cerr << "Unknown argument: --" << name << "\n";
                            return false;
                        }
                    } else {
                        for (size_t j = 1; j < arg.size(); ++j) {
                            char short_name = arg[j];
                            auto it = arg_names_.find(short_name);
                            if (it != arg_names_.end()) {
                                auto& argument = arguments_[it->second];
                                if (argument->isFlag()) {
                                    argument->AddValue("true");
                                } else if (argument->isHelp()) {
                                    help_flag_ = true;
                                    return true;
                                } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                                    std::string_view value = argv[++i];
                                    if (!argument->AddValue(value)) {return false;}
                                    while (argument->isMultiValue() && i + 1 < argc && argv[i + 1][0] != '-') {
                                        if (!argument->AddValue(argv[++i])) {return false;}
                                    }
                                } else {
                                    std::cerr << "Short option requires a value: -" << short_name << "\n";
                                    return false;
                                }
                            } else {
                                std::cerr << "Unknown short option: -" << short_name << "\n";
                                return false;
                            }
                        }
                    }
                }
            } else { 
                bool matched = false;
                for (auto& [name, argument] : arguments_) {
                    if (argument->isPositional()) {
                        argument->AddValue(arg);
                        matched = true;
                        while (argument->isMultiValue() && i + 1 < argc && argv[i + 1][0] != '-') {
                            argument->AddValue(argv[++i]);
                        }
                        break;
                    }
                }
                if (!matched) {
                    std::cerr << "Unexpected positional argument: " << arg << "\n";
                    return false;
                }
            }
        }
        for (auto& [name, argument] : arguments_) {
            if (!argument->isGood() && !argument->isHelp() && argument->isEmpty() && !argument->isFlag()) {
                std::cerr << "Argument " << name << " is missing and has no default value." << std::endl;
                return false;
            }
        }
        return true;
    }


    bool Parse(int argc, char** argv) {
        std::vector<std::string> hohoho;
        for (size_t i = 0; i < argc; ++i) {
            hohoho.push_back(argv[i]);
        }
        return Parse(hohoho);
    }

    template <typename T>
    void StoreValues(std::vector<T>& all_values) {
        for (auto& [name, arg] : arguments_) {
            auto* typed_arg = dynamic_cast<Arg<T>*>(arg.get());
            if (typed_arg) {
                typed_arg->StoreValues(all_values);
            }
        }
    }

    int GetIntValue(std::string_view name, int index = 0) {
        auto arg_name = arguments_.find(name);
        if (arg_name != arguments_.end()) {
            auto* arg = dynamic_cast<Arg<int>*>(arg_name->second.get());
            if (arg) {
                return arg->GetValue(index);
            }
        }
        return index;
    }

    std::string GetStringValue(std::string_view name, int index = 0) {
        auto arg_name = arguments_.find(name);
        if (arg_name != arguments_.end()) {
            auto* arg = dynamic_cast<Arg<std::string>*>(arg_name->second.get());
            if (arg) {
                return arg->GetValue(index);
            }
        }
        return "0";
    }

    bool GetBoolValue(std::string_view name, int index = 0) {
        auto arg_name = arguments_.find(name);
        if (arg_name != arguments_.end()) {
            auto* arg = dynamic_cast<Arg<bool>*>(arg_name->second.get());
            if (arg) {
                return arg->GetValue(index);
            }
        }
        return false;
    }

    bool GetFlag(std::string_view name, int index = 0) {
        auto arg_name = arguments_.find(name);
        if (arg_name != arguments_.end()) {
            auto* arg = dynamic_cast<Arg<bool>*>(arg_name->second.get());
            if (arg) {
                return arg->GetValue(index);
            }
        }
        return false;
    }

    std::string HelpDescription() const {
        std::ostringstream oss;
        oss << "Usage: " << program_name_ << " [options]\n";
        for (const auto& arg : arguments_) {
            oss << "  -" << arg.second->getDescription() << "\n";
        }
        return oss.str();
    }

    bool Help() const {
        return help_flag_;
    }

private:
    std::string_view program_name_;
    std::unordered_map<std::string_view, std::unique_ptr<ArgBase>> arguments_;
    std::unordered_map<char, std::string_view> arg_names_;
    bool help_flag_ = false;

    std::pair<std::string_view, std::string_view> splitString(std::string_view str, char delimeter) {
        size_t pos = str.find(delimeter);
        if (pos == std::string_view::npos) {
            return {str, std::string_view()};
        }
        return {str.substr(0, pos), str.substr(pos + 1)};
    }
};
}