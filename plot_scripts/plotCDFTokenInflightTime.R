#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
file_name <- args[1]
plot_filename <- args[2]
data<-read.csv(file_name)
data$credit <- as.factor(data$credit)


# add a line plot
queue_plot <- ggplot(data, aes_string(x="values",colour="credit")) +
        stat_ecdf(size=1, geom='step') +
        scale_x_continuous(expand=c(0,0)) +
        labs(y="CDF", x="Inflight Time (s)") +
        scale_color_brewer(type = 'div', palette = "RdBu", guide=guide_legend(title=NULL, nrow=2)) +
        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.7)), legend.key.size=unit(10,"points"), legend.position="top")

ggsave(plot_filename, width=5.7, height=5)
