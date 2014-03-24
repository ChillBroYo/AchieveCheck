<?php
/**
 * @file
 * This file deals with the implmentation of the module items described
 * in news_feed.module (Below is a copy of the info from that file)
 *
 * Module capabilities: User able to view a ADMIN UI page in which
 * there will be refresh, update, submit and delete options in a proper format
 * where they will be create content item from a parsed xml source
 *
 * Refresh: Loads all the items from the database in table 'NewsData'
 * onto the Admin UI page for view and selection
 *
 * Update: Parses XML from 
 * https://news.google.com/news/feeds?pz=1&cf=all
 * &ned=us&hl=en&q=Political+party&output=rss
 * and inserts the formatted data into table 'articles' and
 * then calls the refresh function to render the items paged on the page
 *
 * Submit: Creates blog content objects filled with the information from
 * selected items in the admin page and then calls delete on the objects
 * just submitted (once a node is created its info will be deleted in
 * the database)
 *
 * Delete: Removes selected items from the database
 * @link www.achieveinternet.com @endlink 
 *
 * File capabilities: Implements all functionality for the actual news_feed
 * module as well as passes all functionality up to the module
 * 
 * NOTES: there is no implementation of hook_form_alter()
 *
 * @category Feeds
 * @package modules/custom/news_feed
 * @author Samson Tse
 * @license Achieve Internet 
 */

/**
 * Creates the admin form.
 */
function news_feed_admin() {
  return $news_feed_admin_form();
}

/**
 * Admin page builder.
 *
 * Builds the admin page with both dynamic elements and a static 
 * number of clickable buttons.
 */
function news_feed_admin_form() {
  $form = array();

  // Dynamically changing element objects.
  $form['databaseInfo'] = news_feed_refresh();

  // Static control button objects.
  $form['news_feed_ControlButtons'] = news_feed_ControlButtons();
  return $form;
}

/**
 * Parses the unique items from the xml into the database.
 */
function news_feed_XML_store() {

  // Store database values.
  $xml = simplexml_load_file("https://news.google.com/news/feeds?pz=1&cf=all&ned=us&hl=en&q=Political+party&output=rss");
  foreach ($xml->channel->item as $item_value) {

    db_merge('articles')
      ->key(array('guid' => $item_value->guid))
      ->fields(array(
        'title' => $item_value->title,
        'guid' => $item_value->guid,
        'pubdate' => date('Y-m-d H:i:s', strtotime((string) $item_value->pubDate)),
        'link' => $item_value->link,
        'description' => $item_value->description,
        ))
      ->execute();
  }

  // Return the refreshed table value.
  return news_feed_refresh();
}

/**
 * Retrieves the data from the database.
 *
 * Obtains the information from the database tables and returns the 
 * data in a nice checkbox formatted array.
 */
function news_feed_refresh() {
  $header = array(
    'title' => t('Title'),
    'pubdate' => t('Date Published'),
    'description' => t('Summary'),
  );

  // Select the database tables and format the data in a pager-ready format.
  $query = db_select('articles', 'articleInfo')->extend('PagerDefault');
  $query->fields('articleInfo');

  // Adjust pager options.
  $result = $query
    ->limit(2)
    ->orderBy('guid')
    ->execute();

  $rows = array();
  foreach ($result as $row) {
    $rows[] = array('data' => (array) $row);
  }
  $some_options = array();

  // For each individual row, store proper data.
  foreach ($rows as $row) {
    $some_options[$row['data']['guid']] = array(
      'title' => $row['data']['title'],
      'pubdate' => $row['data']['pubdate'],
      'description' => $row['data']['description'],
    );
  }

  // Set additional pager/checkbox table options.
  $form['table'] = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $some_options,
    '#empty' => t('No articles found, please update to preview newest articles from the feed'),
  );

  $form['pager_pager'] = array('#theme' => 'pager');
  return $form;
}

/**
 * Builds the default button objects for manipulating objects in the table.
 */
function news_feed_ControlButtons() {
  $form_buttons = array();

  // Control button 'Update' assignment.
  $form['Ubutton'] = array(
    '#type' => 'submit',
    '#default_value' => 'Update',
    '#required' => TRUE,
    '#submit' => array('news_feed_admin_form_update'),
  );

  // Control button 'Delete' assignment.
  $form['Dbutton'] = array(
    '#type' => 'submit',
    '#default_value' => 'Delete',
    '#required' => TRUE,
    '#submit' => array('news_feed_admin_form_delete'),
  );

  // Control button 'Delete' assignment.
  $form['Sbutton'] = array(
    '#type' => 'submit',
    '#default_value' => 'Submit',
    '#required' => TRUE,
    '#submit' => array('news_feed_admin_form_submit'),
  );

  // Control button 'Refresh' assignment.
  $form['Rbutton'] = array(
    '#type' => 'submit',
    '#default_value' => 'Refresh',
    '#required' => TRUE,
    '#submit' => array('news_feed_admin_form_refresh'),
  );
  return $form;
}

/**
 * Removes items from the database based on the titles selected.
 */
function news_feed_delete_items($titles_to_delete) {
  foreach ($titles_to_delete as $delta => $title) {

    // Delete items from "articles" table.
    db_delete('articles')
      ->condition('title', $title)
      ->execute();
  }
}

/**
 * Creates the content items based on the items selected.
 */
function news_feed_submit_items($nodes_to_submit) {

  // Loop through all nodes' information and create according content.
  foreach ($nodes_to_submit as $delta => $node_data) {

    $node = new stdClass();
    $node->title = $node_data['title'];
    $node->created = strtotime($node_data['pubdate']);
    $node->type = "blog";
    $node->guid = $node_data['guid'];
    $node->format = variable_get('filter_default_format', '');
    $node->language = LANGUAGE_NONE;
    $node->published = 1;
    $node->body[LANGUAGE_NONE][0]['value'] = $node_data['description'];
    $node->body[LANGUAGE_NONE][0]['format'] = 'full_html';
    node_save($node);
  }

  // Display items created.
  if (!empty($nodes_to_submit)) {
    drupal_set_message('You have selected the following items:<br/><br/>'
      . implode($nodes_to_submit, '<br/>'));
  }
}

/**
 * Utilize Drupal submit handler for deletion of items from database tables.
 */
function news_feed_admin_form_delete($form, &$form_state) {
  $values = $form_state['values'];

  // Collect the items selected.
  foreach ($values['table'] as $delta => $item) {

    if (is_string($item)) {

      // If the item is a string store the title of the object.
      $output[] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['title'];
    }
  }

  // Call Delete items function based on titles collected.
  news_feed_delete_items($output);

  drupal_set_message(t('Boom! The item(s) have been deleted.'));
}

/**
 * Utilize Drupal submit handler to update the admin UI with database tables.
 */
function news_feed_admin_form_update($form, &$form_state) {

  // Refresh Current page with database items.
  news_feed_XML_store();

  drupal_set_message(t('Zip! Entries have been retrieved.'));
}

/**
 * Utilize drupal submit handler and refresh items on page.
 */
function news_feed_admin_form_refresh($form, &$form_state) {

  drupal_set_message(t('Entries have been refreshed'));
}

/**
 * Utilize Drupal submit handler to create content.
 */
function news_feed_admin_form_submit($form, &$form_state) {

  $output = array();
  $values = $form_state['values'];
  $items_selected = array();

  // Retrieve data from form for content display.
  foreach ($values['table'] as $delta => $item) {

    if (is_string($item)) {

      $output[$delta]['title'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['title'];
      $output[$delta]['guid'] = $delta;
      $output[$delta]['pubdate'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['pubdate'];
      $output[$delta]['description'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['description'];
    }
  }

  // Send items to function for creation.
  news_feed_submit_items($output);
  drupal_set_message(t('Zing! The content nodes have been created'));
}
