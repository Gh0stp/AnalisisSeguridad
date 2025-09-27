#ifndef REARRANGEDLIST_H
#define REARRANGEDLIST_H

#include <wx/panel.h>
#include <wx/dataview.h>

#include "glue.hpp"
#include "../filezilla/transformed_view.hpp"

class wxButton;

namespace fx::rearranged_list::detail
{
	template <typename T>
	inline T convert(const wxString &s);

	template <>
	inline std::string convert<std::string>(const wxString &s)
	{
		return fz::to_utf8(s);
	}

	template <>
	inline std::wstring convert<std::wstring>(const wxString &s)
	{
		return s.ToStdWstring();
	}

	template <std::size_t I, typename T>
	decltype(auto) get(T && v)
	{
		if constexpr (fz::util::is_like_tuple_with_min_size_v<std::decay_t<T>, 1>) {
			using std::get;

			return get<I>(std::forward<T>(v));
		}
		else
		if constexpr (I == 0) {
			return std::forward<T>(v);
		}
		else {
			static_assert(I == 0, "Index out of bounds.");
		}
	}

	template <typename T, typename SFINAE = void>
	struct tuple_size;

	template <typename T>
	struct tuple_size<T, std::enable_if_t<fz::util::is_like_tuple_with_min_size_v<T, 1>>>: std::tuple_size<T>{};

	template <typename T>
	struct tuple_size<T, std::enable_if_t<!fz::util::is_like_tuple_with_min_size_v<T, 1>>>: std::integral_constant<std::size_t, 1>{};

	template <std::size_t I, typename T, typename SFINAE = void>
	struct tuple_element;

	template <std::size_t I, typename T>
	struct tuple_element<I, T, std::enable_if_t<fz::util::is_like_tuple_with_min_size_v<T, 1>>>: std::tuple_element<I, T>{};

	template <typename T>
	struct tuple_element<0, T, std::enable_if_t<!fz::util::is_like_tuple_with_min_size_v<T, 1>>> {
		using type = T;
	};
}

class RearrangedList: public wxPanel
{
public:
	RearrangedList(wxWindow *parent, const wxString &main_column_name = {}, const std::vector<wxString> extra_column_names = {});

	void SetAvailableItems(std::vector<std::string> items);

	template <template <typename...> typename Container, typename T>
	std::enable_if_t<fz::util::has_iterators<Container<T>>::value>
	SetActiveItems(const Container<T> &items)
	{
		SetActiveItems(items, std::make_index_sequence<fx::rearranged_list::detail::tuple_size<T>::value>());
	}

	template <template <typename...> typename Container, typename T>
	std::enable_if_t<fz::util::is_appendable<Container<T>>::value>
	GetActiveItems(Container<T> &items)
	{
		GetActiveItems(items, std::make_index_sequence<fx::rearranged_list::detail::tuple_size<T>::value>());
	}

	void ClearActiveItems();

private:
	wxSize DoGetBestClientSize() const override;

	template <template <typename...> typename Container, typename T, std::size_t... I>
	void SetActiveItems(const Container<T> &items, const std::integer_sequence<std::size_t, I...>&)
	{
		clear();
		for (auto &i: items) {
			append_item(true, {fz::to_wxString(fx::rearranged_list::detail::get<I>(i))...});
		}
		rearrange();
	}

	template <template <typename...> typename Container, typename T, std::size_t... I>
	void GetActiveItems(Container<T> &items, const std::integer_sequence<std::size_t, I...>&)
	{
		items.clear();
		for (int i=0; i < list_->GetItemCount(); ++i) {
			if (list_->GetToggleValue((unsigned)i, 0)) {
				using fz::toString;
				items.push_back({fx::rearranged_list::detail::convert<typename fx::rearranged_list::detail::tuple_element<I, T>::type>(list_->GetTextValue((unsigned)i, I+1))...});
			}
		}
	}

	void clear();
	void rearrange();
	bool swap_rows(int row1, int row2);
	void move_selected_row(int dir);
	void update_buttons();
	void append_item(bool active, std::initializer_list<wxString> values = {});

	wxDataViewListCtrl *list_{};
	wxButton *up_{};
	wxButton *down_{};

	std::vector<std::string> available_;
	std::vector<std::vector<wxString>> discarded_;
	int row_height_{-1};
	int header_height_{0};
	wxSize best_client_size_{wxDefaultSize};
};

#endif // REARRANGEDLIST_H
