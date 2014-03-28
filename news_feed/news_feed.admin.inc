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
 * and inserts the formatted data into table 'news_feed_articles' and
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

define('NUM_ELEMENTS_PAGED', 2);

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

  // Check if the xml file loaded has anything before pulling data.
  if (isset($xml->channel->item) && !empty($xml->channel->item)) {
    foreach ($xml->channel->item as $item_value) {

      db_merge('news_feed_articles')
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
    'guid' => t('Unique Identifier'),
  );

  // Select the database tables and format the data in a pager-ready format.
  $query = db_select('news_feed_articles', 'articleInfo')->extend('PagerDefault');
  $query->fields('articleInfo');

  // Adjust pager options.
  $result = $query
    ->limit(NUM_ELEMENTS_PAGED)
    ->orderBy('guid')
    ->execute();

  $rows = array();
  foreach ($result as $row) {
    $rows[] = array('data' => (array) $row);
  }
  $some_options = array();

  // For each individual row, store data and differentiate based on guid.
  foreach ($rows as $row) {
    $some_options[$row['data']['guid']] = array(
      'title' => $row['data']['title'],
      'pubdate' => $row['data']['pubdate'],
      'description' => $row['data']['description'],
      'guid' => $row['data']['guid'],
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

  // Control button 'Submit' assignment.
  $form['Sbutton'] = array(
    '#type' => 'submit',
    '#default_value' => 'Submit',
    '#required' => TRUE,
    '#submit' => array('news_feed_admin_form_submit'),
  );

  return $form;
}

/**
 * Removes items from the database based on the titles selected.
 */
function news_feed_delete_item($guid_to_delete) {

  $result = db_select('news_feed_guid', 'guids')
    ->condition('guid', $guid_to_delete, '=')
    ->fields('guids')
    ->execute()
    ->fetchAssoc();

  $nid_to_delete = $result['nid'];

  node_delete($nid_to_delete);

  // Delete items from "news_feed_articles" table.
  db_delete('news_feed_articles')
    ->condition('guid', $guid_to_delete)
    ->execute();

  // Delete items from "news_feed_guid" table.
  db_delete('news_feed_guid')
    ->condition('guid', $guid_to_delete)
    ->execute();
}

/**
 * Creates the content items based on the items selected.
 */
function news_feed_submit_items($nodes_to_submit) {

  // Go through all nodes' information and create according content.

  $node = new stdClass();
  $node->title = $nodes_to_submit['title'];
  $node->created = strtotime($nodes_to_submit['pubdate']);
  $node->type = "blog";
  $node->format = variable_get('filter_default_format', '');
  $node->language = LANGUAGE_NONE;
  $node->published = 1;
  $node->body[LANGUAGE_NONE][0]['value'] = $nodes_to_submit['description'];
  $node->body[LANGUAGE_NONE][0]['format'] = 'full_html';
  node_save($node);

  db_insert('news_feed_guid')
    ->fields(array(
        'guid' => $nodes_to_submit['guid'],
        'nid' => $node->nid,
        ))
      ->execute();
}

/**
 * Utilize Drupal submit handler for deletion of items from database tables.
 */
function news_feed_admin_form_delete($form, &$form_state) {
  $values = $form_state['values'];

  // Collect the items selected.
  foreach ($values['table'] as $delta => $item) {

    if (is_string($item)) {

      $output = array();

      $output = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['guid'];

      // Call Delete items function based on titles collected.
      news_feed_delete_item($output);
    }
  }

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
 * Utilize Drupal submit handler to create content.
 */
function news_feed_admin_form_submit($form, &$form_state) {

  $values = $form_state['values'];

  // Retrieve data from form for content display.
  foreach ($values['table'] as $delta => $item) {

    if (is_string($item)) {

      $output = array();
      $output['title'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['title'];
      $output['pubdate'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['pubdate'];
      $output['description'] = $form_state['complete form']['databaseInfo']['table']['#options'][$delta]['description'];
      $output['guid'] = $delta;
      // Send items to function for creation.
      news_feed_submit_items($output);
    }
  }

  drupal_set_message(t('Zing! The content nodes have been created'));
}
