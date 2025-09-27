#ifndef TEXTVALIDATOREX_H
#define TEXTVALIDATOREX_H

#include <libfilezilla/time.hpp>

#include <wx/valtext.h>

#include <functional>
#include <variant>
#include <set>

#include "../filezilla/util/filesystem.hpp"

namespace fx
{

template <typename T>
struct fielded
{
	std::vector<T> *fields;
	T separators;
	wxString joiners;
};

template <typename T, typename Sep, std::enable_if_t<std::is_constructible_v<T, Sep>>* = nullptr>
fielded(std::vector<T> *fields, Sep separators, wxString joiners) -> fielded<T>;

}

struct TextValidatorEx: wxTextValidator
{
	using ValidatorFunc = std::function<wxString(const wxString &val, bool final_validation)>;

	using Value = std::variant<
		std::nullptr_t,
		std::string *, std::wstring *, wxString *,
		std::vector<std::string> *,
		fx::fielded<std::string>,
		fx::fielded<std::wstring>
	>;

	TextValidatorEx(long style = wxFILTER_NONE, Value value = {}, ValidatorFunc func = {});
	TextValidatorEx(long style, ValidatorFunc func);

private:
	wxObject *Clone() const override;
	wxString IsValid(const wxString& val) const override;
	bool Validate(wxWindow *parent) override;
	bool TransferFromWindow() override;
	bool TransferToWindow() override;

	ValidatorFunc func_;
	bool final_validation_{};

	Value value_ptr_{};
};

wxString ip_validation(const wxString &val, bool final_validation);
TextValidatorEx::ValidatorFunc host_validation(const wxString &field_name = {}, bool at_least_2nd_level = false);

TextValidatorEx::ValidatorFunc absolute_path_validation(fz::util::fs::path_format server_path_format);
TextValidatorEx::ValidatorFunc absolute_uri_validation(std::set<std::string> allowed_schemes = {});
TextValidatorEx::ValidatorFunc field_must_not_be_empty(const wxString &field_name);
TextValidatorEx::ValidatorFunc allow_empty(TextValidatorEx::ValidatorFunc func);

#endif // TEXTVALIDATOREX_H
