#ifndef NAMEDITEMLIST_HPP
#define NAMEDITEMLIST_HPP

#include <memory>

#include <wx/panel.h>
#include <wx/listbase.h>

#include "eventex.hpp"
#include "helpers.hpp"
#include "../filezilla/util/traits.hpp"

class wxButton;

template <typename Char>
class BasicNamedItemList
{
public:
	using char_type = Char;
	using String = std::basic_string<Char>;
	using StringView = std::basic_string_view<Char>;

	virtual ~BasicNamedItemList() = default;

	virtual bool Has(const String &name) = 0;
	virtual bool Append(const String &name, long item_to_copy = -1) = 0;
	virtual bool Remove(long item) = 0;
	virtual bool Rename(long item, const String &new_name) = 0;
	virtual bool IsNameAllowed(const String &name) = 0;
	virtual String CanonicalizeName(const String &name) = 0;
	virtual String GetName(long item) = 0;
	virtual wxItemAttr *GetAttr(long item) = 0;
	virtual std::size_t GetSize() = 0;
};

using NamedItemList = BasicNamedItemList<char>;
using WNamedItemList = BasicNamedItemList<wchar_t>;

template <typename Char>
class BasicNamedItemListManager: public wxPanel
{
public:
	using char_type = Char;
	using String = std::basic_string<Char>;
	using StringView = std::basic_string_view<Char>;

	struct Event;
	BasicNamedItemListManager(wxWindow *parent, const String &default_item_name, const wxString &title);

	void SetList(std::unique_ptr<BasicNamedItemList<Char>> list);
	BasicNamedItemList<Char> *GetList();
	long GetSelectedItem() const;
	size_t size() const;

private:
	class ListCtrl;
	void update_buttons();

	wxButton *add_button_;
	wxButton *remove_button_;
	wxButton *duplicate_button_;
	wxButton *rename_button_;

	ListCtrl *list_;
};

using NamedItemListManager = BasicNamedItemListManager<char>;
using WNamedItemListManager = BasicNamedItemListManager<wchar_t>;

template <typename Char>
struct BasicNamedItemListManager<Char>::Event: wxEventEx<Event>
{
	using Tag = typename wxEventEx<Event>::Tag;

	bool IsAllowed() const {
		return allowed_;
	}

	void Veto() {
		allowed_ = false;
	}

	const String &GetOldName() const {
		return old_name_;
	}

	const String &GetNewName() const {
		return new_name_;
	}

	inline const static Tag SelectedItem;
	inline const static Tag AboutToDeselectItem;
	inline const static Tag Changing;
	inline const static Tag Changed;

private:
	friend Tag;

	using wxEventEx<Event>::wxEventEx;

	Event(const Tag &tag, const String &old_name, const String new_name)
		: wxEventEx<Event>(tag)
		, old_name_(old_name)
		, new_name_(new_name)
	{}

	bool allowed_ = true;

	String old_name_;
	String new_name_;
};

namespace fx::traits {

template <typename T, typename SFINAE = void>
struct has_invalid_chars_in_name: std::false_type{};

template <typename T>
struct has_invalid_chars_in_name<T, std::void_t<decltype(T::invalid_chars_in_name)>>: std::true_type{};

}

template <typename Map>
class MappedItemTraits
{
public:
	using map_type = Map;
	using value_type = typename Map::value_type;
	using mapped_type = typename Map::mapped_type;
	using key_type = typename Map::key_type;

	MappedItemTraits()
	{
		italic_.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT).MakeItalic());
	}

	virtual ~MappedItemTraits() = default;

	virtual bool less_than_item_comparator(value_type &lhs, value_type &rhs)
	{
		return std::forward_as_tuple(is_name_allowed(lhs.first), lhs.first) < std::forward_as_tuple(is_name_allowed(rhs.first), rhs.first);
	}

	virtual wxItemAttr *get_item_attr(value_type &v)
	{
		if (!is_name_allowed(v.first)) {
			return &italic_;
		}

		return nullptr;
	}

	virtual bool is_name_allowed(const key_type &n)
	{
		if (n.empty()) {
			return false;
		}

		if constexpr (fx::traits::has_invalid_chars_in_name<Map>::value) {
			if (n.find_first_of(Map::invalid_chars_in_name) != key_type::npos) {
				return false;
			}
		}

		return true;
	}

	virtual key_type canonicalize_name(const key_type &n)
	{
		if (is_name_allowed(n)) {
			return n;
		}

		return {};
	}

private:
	wxItemAttr italic_;
};

template <typename Map>
class MappedItemListManager: public wxPanel, virtual protected MappedItemTraits<Map>
{
	static_assert(fz::util::is_like_map_v<Map>, "Template parameter must be a map-like type.");
	class List;

public:
	using value_type = typename Map::value_type;
	using mapped_type = typename Map::mapped_type;
	using key_type = typename Map::key_type;
	using map_type = Map;

	using char_type = typename key_type::value_type;
	using String = std::basic_string<char_type>;
	using View = std::basic_string_view<char_type>;

	using Event = typename BasicNamedItemListManager<char_type>::Event;

	MappedItemListManager(wxWindow *parent, const String &default_item_name, const wxString &title)
		: wxPanel(parent)
		, named_manager_(new BasicNamedItemListManager<char_type>(this, default_item_name, title))
	{
		wxVBox(this, 0) = named_manager_;
	}

	value_type *GetSelectedItem() const
	{
		auto list = static_cast<List*>(named_manager_->GetList());
		if (!list) {
			return nullptr;
		}

		return list->GetItem(named_manager_->GetSelectedItem());
	}

	void SetMap(Map &map)
	{
		named_manager_->SetList(std::make_unique<List>(*this, map));
	}

	void ClearMap()
	{
		named_manager_->SetList(nullptr);
	}

	std::size_t size()
	{
		return named_manager_->size();
	}

private:
	BasicNamedItemListManager<char_type> *named_manager_;
};


template <typename Map>
class MappedItemListManager<Map>::List: public BasicNamedItemList<char_type>
{
public:
	List(MappedItemListManager &manager, Map &map)
		: manager_(manager)
		, map_(map)
	{
		item2it_.reserve(map_.size());

		for (auto it = map_.begin(); it != map_.end(); ++it) {
			item2it_.push_back(it);
		}

		std::sort(item2it_.begin(), item2it_.end(), [&](auto &lhs, auto &rhs) {
			return manager_.less_than_item_comparator(*lhs, *rhs);
		});
	}

	value_type *GetItem(long item) const
	{
		if (item < 0 || (std::size_t)item >= item2it_.size()) {
			return {};
		}

		return std::addressof(*item2it_[item]);
	}

private:
	friend MappedItemListManager;

	bool Has(const String &name) override
	{
		return map_.count(name);
	}

	bool Append(const String &name, long item_to_copy) override
	{
		if (item_to_copy >= 0 && (std::size_t)item_to_copy >= item2it_.size()) {
			return false;
		}

		auto copy = [&]() -> const mapped_type * {
			if (item_to_copy < 0) {
				static const mapped_type default_mapped{};
				return &default_mapped;
			}

			return &item2it_[item_to_copy]->second;
		}();

		auto [it, inserted] = map_.try_emplace(std::move(name), *copy);
		if (!inserted) {
			return false;
		}

		item2it_.push_back(it);
		return true;
	}

	bool Remove(long item) override
	{
		if (item < 0 || (std::size_t)item >= item2it_.size()) {
			return false;
		}

		auto it = item2it_[(std::size_t)item];
		map_.erase(it);
		item2it_.erase(item2it_.begin() + item);

		return true;
	}

	bool Rename(long item, const String &new_name) override
	{
		if (item < 0 || (std::size_t)item >= item2it_.size()) {
			return false;
		}

		if (!IsNameAllowed(new_name)) {
			return false;
		}

		if (map_.count(new_name)) {
			return false;
		}

		auto node = map_.extract(item2it_[item]);
		if (!node) {
			return false;
		}

		node.key() =  new_name;

		auto res = map_.insert(std::move(node));
		wxASSERT_MSG(res.inserted, wxS("Inserting the node back into the map failed."));

		item2it_[item] = res.position;

		return true;
	}

	bool IsNameAllowed(const String &name) override
	{
		return manager_.is_name_allowed(name);
	}

	String CanonicalizeName(const String &name) override
	{
		return manager_.canonicalize_name(name);
	}

	String GetName(long item) override
	{
		if (item < 0 || (std::size_t)item >= item2it_.size()) {
			return {};
		}

		return item2it_[item]->first;
	}

	wxListItemAttr *GetAttr(long item) override
	{
		if (item < 0 || (std::size_t)item >= item2it_.size()) {
			return {};
		}

		return manager_.get_item_attr(*item2it_[item]);
	}

	std::size_t GetSize() override
	{
		return item2it_.size();
	}

private:
	MappedItemListManager &manager_;
	Map &map_;

	std::vector<typename Map::iterator> item2it_;
};

extern template class BasicNamedItemList<char>;
extern template class BasicNamedItemListManager<char>;
extern template class BasicNamedItemList<wchar_t>;
extern template class BasicNamedItemListManager<wchar_t>;

#endif // NAMEDITEMLIST_HPP
